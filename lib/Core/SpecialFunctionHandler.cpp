//===-- SpecialFunctionHandler.cpp ----------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "Common.h"

#include "Memory.h"
#include "SpecialFunctionHandler.h"
#include "TimingSolver.h"

#include "klee/ExecutionState.h"
#include <klee/ExprBuilder.h>

#include "klee/Internal/Module/KInstruction.h"
#include "klee/Internal/Module/KModule.h"

#include "Executor.h"
#include "MemoryManager.h"

#include <spa/SPA.h>
#include <spa/PathLoader.h>
#include <spa/Util.h>

#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
#include "llvm/IR/Module.h"
#else
#include "llvm/Module.h"
#endif
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace llvm;
using namespace klee;

/// \todo Almost all of the demands in this file should be replaced
/// with terminateState calls.

///

// FIXME: We are more or less committed to requiring an intrinsic
// library these days. We can move some of this stuff there,
// especially things like realloc which have complicated semantics
// w.r.t. forking. Among other things this makes delayed query
// dispatch easier to implement.
static SpecialFunctionHandler::HandlerInfo handlerInfo[] = {
#define add(name, handler, ret)                                                \
  { name, &SpecialFunctionHandler::handler, false, ret, false }
#define addDNR(name, handler)                                                  \
  { name, &SpecialFunctionHandler::handler, true, false, false }
  addDNR("__assert_rtn", handleAssertFail),
  addDNR("__assert_fail", handleAssertFail), addDNR("_assert", handleAssert),
  addDNR("abort", handleAbort), addDNR("_exit", handleExit),
  { "exit", &SpecialFunctionHandler::handleExit, true, false, true },
  addDNR("klee_abort", handleAbort),
  addDNR("klee_silent_exit", handleSilentExit),
  addDNR("klee_report_error", handleReportError),
  add("calloc", handleCalloc, true), add("free", handleFree, false),
  add("klee_assume", handleAssume, false),
  add("klee_check_memory_access", handleCheckMemoryAccess, false),
  add("klee_get_valuef", handleGetValue, true),
  add("klee_get_valued", handleGetValue, true),
  add("klee_get_valuel", handleGetValue, true),
  add("klee_get_valuell", handleGetValue, true),
  add("klee_get_value_i32", handleGetValue, true),
  add("klee_get_value_i64", handleGetValue, true),
  add("klee_define_fixed_object", handleDefineFixedObject, false),
  add("klee_get_obj_size", handleGetObjSize, true),
  add("klee_get_errno", handleGetErrno, true),
  add("klee_is_symbolic", handleIsSymbolic, true),
  add("klee_make_symbolic", handleMakeSymbolic, false),
  add("klee_mark_global", handleMarkGlobal, false),
  add("klee_merge", handleMerge, false),
  add("klee_prefer_cex", handlePreferCex, false),
  add("klee_print_expr", handlePrintExpr, false),
  add("klee_print_range", handlePrintRange, false),
  add("klee_set_forking", handleSetForking, false),
  add("klee_stack_trace", handleStackTrace, false),
  add("klee_warning", handleWarning, false),
  add("klee_warning_once", handleWarningOnce, false),
  add("klee_alias_function", handleAliasFunction, false),
  add("malloc", handleMalloc, true), add("realloc", handleRealloc, true),

  // operator delete[](void*)
  add("_ZdaPv", handleDeleteArray, false),
  // operator delete(void*)
  add("_ZdlPv", handleDelete, false),

  // operator new[](unsigned int)
  add("_Znaj", handleNewArray, true),
  // operator new(unsigned int)
  add("_Znwj", handleNew, true),

  // FIXME-64: This is wrong for 64-bit long...

  // operator new[](unsigned long)
  add("_Znam", handleNewArray, true),
  // operator new(unsigned long)
  add("_Znwm", handleNew, true),
  add("spa_check_path", handleSpaCheckPath, true),
  add("spa_load_path", handleSpaLoadPath, false),
  add("spa_check_symbol", handleSpaCheckSymbol, true),
  add("spa_seed_symbol", handleSpaSeedSymbol, false),
  add("spa_runtime_call", handleSpaRuntimeCall, false),
  add("spa_snprintf5", handleSpaSnprintf5, false),
#undef addDNR
#undef add
};

SpecialFunctionHandler::const_iterator SpecialFunctionHandler::begin() {
  return SpecialFunctionHandler::const_iterator(handlerInfo);
}

SpecialFunctionHandler::const_iterator SpecialFunctionHandler::end() {
  // NULL pointer is sentinel
  return SpecialFunctionHandler::const_iterator(0);
}

SpecialFunctionHandler::const_iterator &
SpecialFunctionHandler::const_iterator::operator++() {
  ++index;
  if (index >= SpecialFunctionHandler::size()) {
    // Out of range, return .end()
    base = 0; // Sentinel
    index = 0;
  }

  return *this;
}

int SpecialFunctionHandler::size() {
  return sizeof(handlerInfo) / sizeof(handlerInfo[0]);
}

SpecialFunctionHandler::SpecialFunctionHandler(Executor &_executor)
    : executor(_executor) {}

void SpecialFunctionHandler::prepare() {
  unsigned N = size();

  for (unsigned i = 0; i < N; ++i) {
    HandlerInfo &hi = handlerInfo[i];
    Function *f = executor.kmodule->module->getFunction(hi.name);

    // No need to create if the function doesn't exist, since it cannot
    // be called in that case.

    if (f && (!hi.doNotOverride || f->isDeclaration())) {
      // Make sure NoReturn attribute is set, for optimization and
      // coverage counting.
      if (hi.doesNotReturn)
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 3)
        f->addFnAttr(Attribute::NoReturn);
#elif LLVM_VERSION_CODE >= LLVM_VERSION(3, 2)
      f->addFnAttr(Attributes::NoReturn);
#else
      f->addFnAttr(Attribute::NoReturn);
#endif

      // Change to a declaration since we handle internally (simplifies
      // module and allows deleting dead code).
      if (!f->isDeclaration())
        f->deleteBody();
    }
  }
}

void SpecialFunctionHandler::bind() {
  unsigned N = sizeof(handlerInfo) / sizeof(handlerInfo[0]);

  for (unsigned i = 0; i < N; ++i) {
    HandlerInfo &hi = handlerInfo[i];
    Function *f = executor.kmodule->module->getFunction(hi.name);

    if (f && (!hi.doNotOverride || f->isDeclaration()))
      handlers[f] = std::make_pair(hi.handler, hi.hasReturnValue);
  }
}

bool SpecialFunctionHandler::handle(ExecutionState &state, Function *f,
                                    KInstruction *target,
                                    std::vector<ref<Expr> > &arguments) {
  handlers_ty::iterator it = handlers.find(f);
  if (it != handlers.end()) {
    Handler h = it->second.first;
    bool hasReturnValue = it->second.second;
    // FIXME: Check this... add test?
    if (!hasReturnValue && !target->inst->use_empty()) {
      executor.terminateStateOnExecError(
          state, "expected return value from void special function");
    } else {
      (this->*h)(state, target, arguments);
    }
    return true;
  } else {
    return false;
  }
}

/****/

// reads a concrete string from memory
std::string SpecialFunctionHandler::readStringAtAddress(ExecutionState &state,
                                                        ref<Expr> addressExpr) {
  ObjectPair op;
  addressExpr = executor.toUnique(state, addressExpr);
  ref<ConstantExpr> address = cast<ConstantExpr>(addressExpr);
  if (!state.addressSpace.resolveOne(address, op))
    assert(0 && "XXX out of bounds / multiple resolution unhandled");
  bool res;
  assert(executor.solver->mustBeTrue(
      state, EqExpr::create(address, op.first->getBaseExpr()), res) && res &&
         "XXX interior pointer unhandled");
  const MemoryObject *mo = op.first;
  const ObjectState *os = op.second;

  char *buf = new char[mo->size];

  unsigned i;
  for (i = 0; i < mo->size - 1; i++) {
    ref<Expr> cur = os->read8(i);
    cur = executor.toUnique(state, cur);
    assert(isa<ConstantExpr>(cur) &&
           "hit symbolic char while reading concrete string");
    buf[i] = cast<ConstantExpr>(cur)->getZExtValue(8);
  }
  buf[i] = 0;

  std::string result(buf);
  delete[] buf;
  return result;
}

/****/

void SpecialFunctionHandler::handleAbort(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 0 && "invalid number of arguments to abort");

  //XXX:DRE:TAINT
  if (state.underConstrained) {
    llvm::errs() << "TAINT: skipping abort fail\n";
    executor.terminateState(state);
  } else {
    executor.terminateStateOnError(state, "abort failure", "abort.err");
  }
}

void SpecialFunctionHandler::handleExit(ExecutionState &state,
                                        KInstruction *target,
                                        std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to exit");
  executor.terminateStateOnExit(state);
}

void
SpecialFunctionHandler::handleSilentExit(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to exit");
  executor.terminateState(state);
}

void SpecialFunctionHandler::handleAliasFunction(
    ExecutionState &state, KInstruction *target,
    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "invalid number of arguments to klee_alias_function");
  std::string old_fn = readStringAtAddress(state, arguments[0]);
  std::string new_fn = readStringAtAddress(state, arguments[1]);
  DEBUG_WITH_TYPE(
      "alias_handling",
      llvm::errs() << "Replacing " << old_fn << "() with " << new_fn << "()\n");
  if (old_fn == new_fn)
    state.removeFnAlias(old_fn);
  else
    state.addFnAlias(old_fn, new_fn);
}

void SpecialFunctionHandler::handleAssert(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 3 && "invalid number of arguments to _assert");

  //XXX:DRE:TAINT
  if (state.underConstrained) {
    llvm::errs() << "TAINT: skipping assertion:" << readStringAtAddress(
                                                        state, arguments[0])
                 << "\n";
    executor.terminateState(state);
  } else
    executor.terminateStateOnError(
        state, "ASSERTION FAIL: " + readStringAtAddress(state, arguments[0]),
        "assert.err");
}

void
SpecialFunctionHandler::handleAssertFail(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 4 &&
         "invalid number of arguments to __assert_fail");

  //XXX:DRE:TAINT
  if (state.underConstrained) {
    llvm::errs() << "TAINT: skipping assertion:" << readStringAtAddress(
                                                        state, arguments[0])
                 << "\n";
    executor.terminateState(state);
  } else
    executor.terminateStateOnError(
        state, "ASSERTION FAIL: " + readStringAtAddress(state, arguments[0]),
        "assert.err");
}

void
SpecialFunctionHandler::handleReportError(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 4 &&
         "invalid number of arguments to klee_report_error");

  // arguments[0], arguments[1] are file, line

  //XXX:DRE:TAINT
  if (state.underConstrained) {
    llvm::errs()
        << "TAINT: skipping klee_report_error:" << readStringAtAddress(
                                                       state, arguments[2])
        << ":" << readStringAtAddress(state, arguments[3]) << "\n";
    executor.terminateState(state);
  } else
    executor.terminateStateOnError(
        state, readStringAtAddress(state, arguments[2]),
        readStringAtAddress(state, arguments[3]).c_str());
}

void SpecialFunctionHandler::handleMerge(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  // nop
}

void SpecialFunctionHandler::handleNew(ExecutionState &state,
                                       KInstruction *target,
                                       std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to new");

  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleDelete(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  // FIXME: Should check proper pairing with allocation type (malloc/free,
  // new/delete, new[]/delete[]).

  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to delete");
  executor.executeFree(state, arguments[0]);
}

void
SpecialFunctionHandler::handleNewArray(ExecutionState &state,
                                       KInstruction *target,
                                       std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to new[]");
  executor.executeAlloc(state, arguments[0], false, target);
}

void
SpecialFunctionHandler::handleDeleteArray(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to delete[]");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleMalloc(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to malloc");
  executor.executeAlloc(state, arguments[0], false, target);
}

void SpecialFunctionHandler::handleAssume(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 && "invalid number of arguments to klee_assume");

  ref<Expr> e = arguments[0];

  if (e->getWidth() != Expr::Bool)
    e = NeExpr::create(e, ConstantExpr::create(0, e->getWidth()));

  bool res;
  bool success = executor.solver->mustBeFalse(state, e, res);
  assert(success && "FIXME: Unhandled solver failure");
  if (res) {
    executor.terminateStateOnError(
        state, "invalid klee_assume call (provably false)", "user.err");
  } else {
    executor.addConstraint(state, e);
  }
}

void
SpecialFunctionHandler::handleIsSymbolic(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "invalid number of arguments to klee_is_symbolic");

  executor.bindLocal(
      target, state,
      ConstantExpr::create(!isa<ConstantExpr>(arguments[0]), Expr::Int32));
}

void
SpecialFunctionHandler::handlePreferCex(ExecutionState &state,
                                        KInstruction *target,
                                        std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "invalid number of arguments to klee_prefex_cex");

  ref<Expr> cond = arguments[1];
  if (cond->getWidth() != Expr::Bool)
    cond = NeExpr::create(cond, ConstantExpr::alloc(0, cond->getWidth()));

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "prefex_cex");

  assert(rl.size() == 1 &&
         "prefer_cex target must resolve to precisely one object");

  rl[0].first.first->cexPreferences.push_back(cond);
}

void
SpecialFunctionHandler::handlePrintExpr(ExecutionState &state,
                                        KInstruction *target,
                                        std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "invalid number of arguments to klee_print_expr");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  llvm::errs() << msg_str << ":" << arguments[1] << "\nGiven:\n";
  state.constraints.dump();
}

void
SpecialFunctionHandler::handleSetForking(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "invalid number of arguments to klee_set_forking");
  ref<Expr> value = executor.toUnique(state, arguments[0]);

  if (ConstantExpr *CE = dyn_cast<ConstantExpr>(value)) {
    state.forkDisabled = CE->isZero();
  } else {
    executor.terminateStateOnError(
        state, "klee_set_forking requires a constant arg", "user.err");
  }
}

void
SpecialFunctionHandler::handleStackTrace(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  state.dumpStack(outs());
}

void SpecialFunctionHandler::handleWarning(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "invalid number of arguments to klee_warning");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  klee_warning("%s: %s", state.stack.back().kf->function->getName().data(),
               msg_str.c_str());
}

void
SpecialFunctionHandler::handleWarningOnce(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "invalid number of arguments to klee_warning_once");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  klee_warning_once(0, "%s: %s",
                    state.stack.back().kf->function->getName().data(),
                    msg_str.c_str());
}

void
SpecialFunctionHandler::handlePrintRange(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "invalid number of arguments to klee_print_range");

  std::string msg_str = readStringAtAddress(state, arguments[0]);
  llvm::errs() << msg_str << ":" << arguments[1];
  if (!isa<ConstantExpr>(arguments[1])) {
    // FIXME: Pull into a unique value method?
    ref<ConstantExpr> value;
    bool success = executor.solver->getValue(state, arguments[1], value);
    assert(success && "FIXME: Unhandled solver failure");
    bool res;
    success = executor.solver
        ->mustBeTrue(state, EqExpr::create(arguments[1], value), res);
    assert(success && "FIXME: Unhandled solver failure");
    if (res) {
      llvm::errs() << " == " << value;
    } else {
      llvm::errs() << " ~= " << value;
      std::pair<ref<Expr>, ref<Expr> > res =
          executor.solver->getRange(state, arguments[1]);
      llvm::errs() << " (in [" << res.first << ", " << res.second << "])";
    }
  }
  llvm::errs() << "\n";
}

void
SpecialFunctionHandler::handleGetObjSize(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 1 &&
         "invalid number of arguments to klee_get_obj_size");
  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "klee_get_obj_size");
  for (Executor::ExactResolutionList::iterator it = rl.begin(), ie = rl.end();
       it != ie; ++it) {
    executor.bindLocal(
        target, *it->second,
        ConstantExpr::create(it->first.first->size, Expr::Int32));
  }
}

void
SpecialFunctionHandler::handleGetErrno(ExecutionState &state,
                                       KInstruction *target,
                                       std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 0 &&
         "invalid number of arguments to klee_get_errno");
  executor.bindLocal(target, state, ConstantExpr::create(errno, Expr::Int32));
}

void SpecialFunctionHandler::handleCalloc(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 2 && "invalid number of arguments to calloc");

  ref<Expr> size = MulExpr::create(arguments[0], arguments[1]);
  executor.executeAlloc(state, size, false, target, true);
}

void SpecialFunctionHandler::handleRealloc(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 2 && "invalid number of arguments to realloc");
  ref<Expr> address = arguments[0];
  ref<Expr> size = arguments[1];

  Executor::StatePair zeroSize =
      executor.fork(state, Expr::createIsZero(size), true);

  if (zeroSize.first) { // size == 0
    executor.executeFree(*zeroSize.first, address, target);
  }
  if (zeroSize.second) { // size != 0
    Executor::StatePair zeroPointer =
        executor.fork(*zeroSize.second, Expr::createIsZero(address), true);

    if (zeroPointer.first) { // address == 0
      executor.executeAlloc(*zeroPointer.first, size, false, target);
    }
    if (zeroPointer.second) { // address != 0
      Executor::ExactResolutionList rl;
      executor.resolveExact(*zeroPointer.second, address, rl, "realloc");

      for (Executor::ExactResolutionList::iterator it = rl.begin(),
                                                   ie = rl.end();
           it != ie; ++it) {
        executor.executeAlloc(*it->second, size, false, target, false,
                              it->first.second);
      }
    }
  }
}

void SpecialFunctionHandler::handleFree(ExecutionState &state,
                                        KInstruction *target,
                                        std::vector<ref<Expr> > &arguments) {
  // XXX should type check args
  assert(arguments.size() == 1 && "invalid number of arguments to free");
  executor.executeFree(state, arguments[0]);
}

void SpecialFunctionHandler::handleCheckMemoryAccess(
    ExecutionState &state, KInstruction *target,
    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "invalid number of arguments to klee_check_memory_access");

  ref<Expr> address = executor.toUnique(state, arguments[0]);
  ref<Expr> size = executor.toUnique(state, arguments[1]);
  if (!isa<ConstantExpr>(address) || !isa<ConstantExpr>(size)) {
    executor.terminateStateOnError(
        state, "check_memory_access requires constant args", "user.err");
  } else {
    ObjectPair op;

    if (!state.addressSpace.resolveOne(cast<ConstantExpr>(address), op)) {
      executor.terminateStateOnError(state, "check_memory_access: memory error",
                                     "ptr.err",
                                     executor.getAddressInfo(state, address));
    } else {
      ref<Expr> chk = op.first->getBoundsCheckPointer(
          address, cast<ConstantExpr>(size)->getZExtValue());
      if (!chk->isTrue()) {
        executor.terminateStateOnError(
            state, "check_memory_access: memory error", "ptr.err",
            executor.getAddressInfo(state, address));
      }
    }
  }
}

void
SpecialFunctionHandler::handleGetValue(ExecutionState &state,
                                       KInstruction *target,
                                       std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "invalid number of arguments to klee_get_value");

  executor.executeGetValue(state, arguments[0], target);
}

void SpecialFunctionHandler::handleDefineFixedObject(
    ExecutionState &state, KInstruction *target,
    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "invalid number of arguments to klee_define_fixed_object");
  assert(isa<ConstantExpr>(arguments[0]) &&
         "expect constant address argument to klee_define_fixed_object");
  assert(isa<ConstantExpr>(arguments[1]) &&
         "expect constant size argument to klee_define_fixed_object");

  uint64_t address = cast<ConstantExpr>(arguments[0])->getZExtValue();
  uint64_t size = cast<ConstantExpr>(arguments[1])->getZExtValue();
  MemoryObject *mo =
      executor.memory->allocateFixed(address, size, state.prevPC->inst);
  executor.bindObjectInState(state, mo, false);
  mo->isUserSpecified = true; // XXX hack;
}

void
SpecialFunctionHandler::handleMakeSymbolic(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  std::string name;

  // FIXME: For backwards compatibility, we should eventually enforce the
  // correct arguments.
  if (arguments.size() == 2) {
    name = "unnamed";
  } else {
    // FIXME: Should be a user.err, not an assert.
    assert(arguments.size() == 3 &&
           "invalid number of arguments to klee_make_symbolic");
    name = readStringAtAddress(state, arguments[2]);
  }

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "make_symbolic");

  for (Executor::ExactResolutionList::iterator it = rl.begin(), ie = rl.end();
       it != ie; ++it) {
    const MemoryObject *mo = it->first.first;
    mo->setName(name);

    const ObjectState *old = it->first.second;
    ExecutionState *s = it->second;

    if (old->readOnly) {
      executor.terminateStateOnError(*s, "cannot make readonly object symbolic",
                                     "user.err");
      return;
    }

    // FIXME: Type coercion should be done consistently somewhere.
    bool res;
    bool success = executor.solver->mustBeTrue(
        *s, EqExpr::create(ZExtExpr::create(arguments[1],
                                            Context::get().getPointerWidth()),
                           mo->getSizeExpr()),
        res);
    assert(success && "FIXME: Unhandled solver failure");

    if (res) {
      executor.executeMakeSymbolic(*s, mo, name);
    } else {
      executor.terminateStateOnError(
          *s, "wrong size given to klee_make_symbolic[_name]", "user.err");
    }
  }
}

void
SpecialFunctionHandler::handleMarkGlobal(ExecutionState &state,
                                         KInstruction *target,
                                         std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "invalid number of arguments to klee_mark_global");

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "mark_global");

  for (Executor::ExactResolutionList::iterator it = rl.begin(), ie = rl.end();
       it != ie; ++it) {
    const MemoryObject *mo = it->first.first;
    assert(!mo->isLocal);
    mo->isGlobal = true;
  }
}

namespace SPA {
llvm::cl::opt<std::string> ParticipantName(
    "participant",
    llvm::cl::desc("Sets the participant name (default: module-name)."));

llvm::cl::opt<std::string>
    IP("ip", llvm::cl::desc("Sets the participant IP address when bind doesn't "
                            "(default: 127.0.0.1)."));

llvm::cl::opt<bool> ShallowExploration(
    "shallow-exploration",
    llvm::cl::desc("Only perform shallow exploration (initial or immediate "
                   "responses). spa-explore-conversation can fill in the "
                   "deeper conversations as a separate process."));

PathLoader *senderPaths = NULL;
bool followSenderPaths = false;
std::map<std::string, std::string> seedSymbolMappings;
bool connectSockets = false;
}

void
SpecialFunctionHandler::handleSpaCheckPath(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "Invalid number of arguments to spa_check_path.");

  klee::ConstantExpr *ce;
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[0])));
  uint64_t pathID = ce->getZExtValue();

  if (!SPA::senderPaths) {
    klee_message("[spa_check_path] No sender path-file. Nothing to load.");
    assert(pathID == 0 &&
           "Checking for non-zero pathID without sender path-file.");
    executor.bindLocal(target, state, ConstantExpr::create(false, Expr::Int32));
  } else if (SPA::followSenderPaths) {
    klee_message("[spa_check_path] Following sender path-file. Allowing path "
                 "%ld to be loaded. Path may not be ready yet.",
                 pathID);
    executor.bindLocal(target, state, ConstantExpr::create(true, Expr::Int32));
  } else {
    if (SPA::senderPaths->getPath(pathID)) {
      klee_message("[spa_check_path] Finite sender path-file. Allowing path "
                   "%ld to be loaded.",
                   pathID);
      executor.bindLocal(target, state,
                         ConstantExpr::create(true, Expr::Int32));
    } else {
      klee_message("[spa_check_path] Finished processing sender "
                   "path-file. Path %ld out-of-bound. Terminating.",
                   pathID);
      executor.terminateStateOnError(state, "Path ID out-of-bounds.",
                                     "user.err");
    }
  }
}

void
SpecialFunctionHandler::handleSpaLoadPath(ExecutionState &state,
                                          KInstruction *target,
                                          std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "Invalid number of arguments to spa_load_path.");

  klee::ConstantExpr *ce;
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[0])));
  uint64_t pathID = ce->getZExtValue();

  if (!SPA::senderPaths) {
    klee_message(
        "[spa_load_path] No sender path-file specified. Not loading path.");
    assert(pathID == 0 &&
           "Loading non-zero pathID without a sender path-file.");
    return;
  }

  assert((!state.senderPath) && "Path already loaded.");

  klee_message("[spa_load_path]   Loading path %ld from file.", pathID);
  while ((!(state.senderPath.reset(SPA::senderPaths->getPath(pathID)),
            state.senderPath)) && SPA::followSenderPaths) {
    klee_message("[spa_load_path]     Path %ld not yet available. Waiting.",
                 pathID);
    sleep(1);
  }
  assert(state.senderPath &&
         "Attempting to process seed path before it's available.");

  klee_message(
      "[spa_load_path]   Starting joint symbolic execution on path %ld (%s).",
      pathID, state.senderPath->getUUID().c_str());

  state.senderLogPos = state.senderPath->getSymbolLog().begin();

  // Get current participant's IP/Ports.
  std::set<std::string> participantBindings;
  for (auto sit : state.senderPath->getSymbolLog()) {
    if (sit->isMessage() && sit->getParticipant() == SPA::ParticipantName) {
      std::string binding;
      if (sit->isInput()) {
        binding =
            sit->getMessageDestinationIP() + ":" + sit->getMessageProtocol() +
            ":" + sit->getMessageDestinationPort();
      } else if (sit->isOutput()) {
        binding = sit->getMessageSourceIP() + ":" + sit->getMessageProtocol() +
                  ":" + sit->getMessageSourcePort();
      }
      if (!participantBindings.count(binding)) {
        klee_message("[spa_load_path]   Participant accepts messages for %s.",
                     binding.c_str());
        participantBindings.insert(binding);
      }
    }
  }
  if (!SPA::IP.empty()) {
    klee_message("[spa_load_path]   Participant accepts messages for %s:*.",
                 SPA::IP.c_str());
    participantBindings.insert(SPA::IP + ":*");
  }
  // Check if there are any symbols in the log that can be consumed but that
  // come after any symbols outputted by the current participant.
  // To check this, check the log in reverse and find the first of either a
  // symbol outputted by the current participant or a symbol that can be
  // consumed.
  // If performing shallow exploration, only consider the most recent
  // participant if it was not derived (if it was derived then the source will
  // be explored and this contribution can be derived from that).
  bool receivingSymbols = false;
  for (auto sit = state.senderPath->getSymbolLog().rbegin(),
            sie = state.senderPath->getSymbolLog().rend();
       sit != sie; sit++) {
    // Only consider last participant for shallow exploration.
    if (SPA::ShallowExploration &&
        ((!state.senderPath->getDerivedFromUUID().empty()) ||
         ((!state.senderPath->getParticipants().empty()) &&
          (*sit)->getParticipant() !=
              state.senderPath->getParticipants().back()->getName()))) {
      klee_message("[spa_load_path] Cannot load path with no shallow inputs. "
                   "Terminating.");
      executor.terminateStateOnError(state, "Path has no shallow inputs.",
                                     "user.err");
      return;
    }

    // Check if symbol can be consumed by a direct mapping.
    if (SPA::seedSymbolMappings.count((*sit)->getQualifiedName())) {
      receivingSymbols = true;
      break;
    }
    // Check if symbol can be consumed via symbolic socket layer.
    if (SPA::connectSockets && (*sit)->isOutput() && (*sit)->isMessage() &&
        (!(*sit)->isDropped()) &&
        (participantBindings.count((*sit)->getMessageDestinationIP() + ":" +
                                   (*sit)->getMessageProtocol() + ":" +
                                   (*sit)->getMessageDestinationPort()) ||
         participantBindings.count((*sit)->getMessageDestinationIP() + ":*"))) {
      receivingSymbols = true;
      break;
    }
    // Check if symbol was generated by current participant.
    if ((*sit)->getParticipant() == SPA::ParticipantName) {
      klee_message(
          "[spa_load_path] Cannot load path with no new inputs. Terminating.");
      executor.terminateStateOnError(state, "Path has no new inputs.",
                                     "user.err");
      return;
    }
  }
  if (SPA::ShallowExploration && (!receivingSymbols) &&
      (!state.senderPath->getSymbolLog().empty())) {
    klee_message("[spa_load_path] Cannot load path with no shallow inputs. "
                 "Terminating.");
    executor.terminateStateOnError(state, "Path has no shallow inputs.",
                                   "user.err");
    return;
  }

  // Add sender path constraints.
  klee_message("[spa_load_path]   ANDing in sender path constraints.");
  for (auto it : state.senderPath->getConstraints()) {
    // Don't add internal symbols as they may collide.
    if (klee::EqExpr *eqExpr = llvm::dyn_cast<klee::EqExpr>(it)) {
      if (klee::ConcatExpr *catExpr =
              llvm::dyn_cast<klee::ConcatExpr>(eqExpr->right)) {
        if (klee::ReadExpr *rdExpr =
                llvm::dyn_cast<klee::ReadExpr>(catExpr->getLeft())) {
          if (rdExpr->updates.root->name.compare(0, strlen(SPA_INTERNAL_PREFIX),
                                                 SPA_INTERNAL_PREFIX) == 0) {
            continue;
          }
        }
      }
    }
    if (!state.addAndCheckConstraint(it)) {
      executor.terminateStateOnError(
          state, "Loading path made constraints trivially UNSAT "
                 "(incompatible message for this path).",
          "user.err");
      return;
    }
  }
}

void SpecialFunctionHandler::handleSpaCheckSymbol(
    ExecutionState &state, KInstruction *target,
    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "Invalid number of arguments to spa_check_symbol.");

  std::string localName = readStringAtAddress(state, arguments[0]);
  std::string qualifiedName =
      localName + SPA_SYMBOL_DELIMITER + SPA::ParticipantName;

  klee::ConstantExpr *ce;
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[1])));
  uint64_t pathID = ce->getZExtValue();

  klee_message(
      "[spa_check_symbol] Checking availability of symbol %s in path %ld.",
      localName.c_str(), pathID);

  if (!SPA::senderPaths) {
    klee_message("[spa_check_symbol] Not performing joint symbolic execution. "
                 "Considering %s as available.",
                 localName.c_str());
    assert(pathID == 0 && "Non-zero pathID without a sender path-file.");
    executor.bindLocal(target, state, ConstantExpr::create(0, Expr::Int32));
    return;
  }

  assert(state.senderPath && "Checking symbol before path has been loaded.");

  // Check if input mapped.
  auto senderLogPos = state.senderLogPos;
  if (SPA::seedSymbolMappings.count(qualifiedName)) { // Explicit mapping.
    // Skip log entries until named symbol.
    for (; senderLogPos != state.senderPath->getSymbolLog().end();
         senderLogPos++) {
      klee_message(
          "[spa_check_symbol]   Checking log entry %d: %s",
          (int)(senderLogPos - state.senderPath->getSymbolLog().begin()),
          (*senderLogPos)->getFullName().c_str());

      // Check for outstanding outputs in log.
      if ((*senderLogPos)->isOutput() &&
          (*senderLogPos)->getParticipant() == SPA::ParticipantName) {
        bool outputSent = false;
        for (auto it : state.symbolics) {
          if (it.second->name == (*senderLogPos)->getFullName()) {
            klee_message("[spa_check_symbol]      Output %s already sent.",
                         (*senderLogPos)->getFullName().c_str());
            outputSent = true;
            break;
          }
        }
        if (!outputSent) {
          klee_message("[spa_check_symbol]   Trying to receive %s before "
                       "sending %s. Considering input as not yet available.",
                       qualifiedName.c_str(),
                       (*senderLogPos)->getFullName().c_str());
          executor.bindLocal(target, state,
                             ConstantExpr::alloc(llvm::APInt(32, -1, true)));
          return;
        }
      }

      if ((*senderLogPos)->getQualifiedName() ==
          SPA::seedSymbolMappings[qualifiedName]) {
        break;
      }
    }
  } else if (SPA::connectSockets) {
    if (localName.compare(0, strlen(SPA_MESSAGE_INPUT_PREFIX),
                          SPA_MESSAGE_INPUT_PREFIX) == 0) {
      // Socket mapping in=out.
      // Skip log entries until compatible symbol.
      for (; senderLogPos != state.senderPath->getSymbolLog().end();
           senderLogPos++) {
        klee_message(
            "[spa_check_symbol]   Checking log entry %d: %s",
            (int)(senderLogPos - state.senderPath->getSymbolLog().begin()),
            (*senderLogPos)->getFullName().c_str());

        // Check for outstanding outputs in log.
        if ((*senderLogPos)->isOutput() &&
            (*senderLogPos)->getParticipant() == SPA::ParticipantName) {
          bool outputSent = false;
          for (auto it : state.symbolics) {
            if (it.second->name == (*senderLogPos)->getFullName()) {
              klee_message("[spa_check_symbol]      Output %s already sent.",
                           (*senderLogPos)->getFullName().c_str());
              outputSent = true;
              break;
            }
          }
          if (!outputSent) {
            klee_message("[spa_check_symbol]   Trying to receive %s before "
                         "sending %s. Considering input as not yet available.",
                         qualifiedName.c_str(),
                         (*senderLogPos)->getFullName().c_str());
            executor.bindLocal(target, state,
                               ConstantExpr::alloc(llvm::APInt(32, -1, true)));
            return;
          }
        }

        if ((*senderLogPos)->isOutput() && (*senderLogPos)->isMessage() &&
            checkMessageCompatibility(*senderLogPos, localName)) {
          break;
        }
      }
    } else if (localName.compare(0, strlen(SPA_API_INPUT_PREFIX),
                                 SPA_API_INPUT_PREFIX) == 0 ||
               localName.compare(0, strlen(SPA_MODEL_INPUT_PREFIX),
                                 SPA_MODEL_INPUT_PREFIX) == 0) {
      // API/Model mapping in=in.
      // Skip log entries until same symbol.
      for (; senderLogPos != state.senderPath->getSymbolLog().end();
           senderLogPos++) {
        klee_message(
            "[spa_check_symbol]   Checking log entry %d: %s",
            (int)(senderLogPos - state.senderPath->getSymbolLog().begin()),
            (*senderLogPos)->getFullName().c_str());

        // Check for outstanding outputs in log.
        if ((*senderLogPos)->isOutput() &&
            (*senderLogPos)->getParticipant() == SPA::ParticipantName) {
          bool outputSent = false;
          for (auto it : state.symbolics) {
            if (it.second->name == (*senderLogPos)->getFullName()) {
              klee_message("[spa_check_symbol]      Output %s already sent.",
                           (*senderLogPos)->getFullName().c_str());
              outputSent = true;
              break;
            }
          }
          if (!outputSent) {
            klee_message("[spa_check_symbol]   Trying to receive %s before "
                         "sending %s. Considering input as not yet available.",
                         qualifiedName.c_str(),
                         (*senderLogPos)->getFullName().c_str());
            executor.bindLocal(target, state,
                               ConstantExpr::alloc(llvm::APInt(32, -1, true)));
            return;
          }
        }

        //TODO: Compare sequence number (full-name) if ever available.
        if (qualifiedName == (*senderLogPos)->getQualifiedName()) {
          break;
        }
      }
    }
  } else { // Not mapped, leave symbol unconstrained.
    klee_message(
        "[spa_check_symbol] %s is not connected. Considering as available.",
        qualifiedName.c_str());
    executor.bindLocal(target, state, ConstantExpr::create(0, Expr::Int32));
    return;
  }

  // Check if log ran out.
  if (senderLogPos == state.senderPath->getSymbolLog().end()) {
    klee_message("[spa_check_symbol] %s is not available in log.",
                 qualifiedName.c_str());
    executor.bindLocal(target, state,
                       ConstantExpr::alloc(llvm::APInt(32, -1, true)));
  } else {
    klee_message("[spa_check_symbol] %s is available in log after %d entries.",
                 qualifiedName.c_str(),
                 (int)(senderLogPos - state.senderLogPos));
    executor.bindLocal(
        target, state,
        ConstantExpr::create(senderLogPos - state.senderLogPos, Expr::Int32));
  }
}

void SpecialFunctionHandler::handleSpaSeedSymbol(
    ExecutionState &state, KInstruction *target,
    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 2 &&
         "Invalid number of arguments to spa_seed_symbol.");

  klee::ConstantExpr *ce;
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[1])));
  uint64_t pathID = ce->getZExtValue();

  if (!SPA::senderPaths) {
    klee_message("[spa_seed_symbol] Not performing joint symbolic execution. "
                 "Not seeding symbol.");
    assert(pathID == 0 &&
           "Seeding non-zero pathID without a sender path-file.");
    return;
  }

  assert(state.senderPath && "Seeding symbol before path has been loaded.");

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "seed_symbol");
  assert(rl.size() == 1 &&
         "Seeding symbol that doesn't resolve to a single object.");
  const MemoryObject *mo = rl[0].first.first;
  ObjectState *os = const_cast<ObjectState *>(rl[0].first.second);
  assert(!os->readOnly && "Seeding read-only object.");
  std::string fullName = mo->name;
  size_t size = mo->getSizeExpr()->getZExtValue(sizeof(size_t) * 8);

  klee_message("[spa_seed_symbol] Seeding path %ld, on input: %s", pathID,
               fullName.c_str());

  // Drop the participant and sequence number.
  std::string qualifiedName =
      fullName.substr(0, fullName.rfind(SPA_SYMBOL_DELIMITER));
  std::string localName =
      qualifiedName.substr(0, qualifiedName.rfind(SPA_SYMBOL_DELIMITER));

  // Check if input mapped.
  if (SPA::seedSymbolMappings.count(qualifiedName)) { // Explicit mapping.
    // Skip log entries until named symbol.
    for (; state.senderLogPos != state.senderPath->getSymbolLog().end();
         state.senderLogPos++) {
      klee_message(
          "[spa_seed_symbol]   Checking log entry %d: %s",
          (int)(state.senderLogPos - state.senderPath->getSymbolLog().begin()),
          (*state.senderLogPos)->getFullName().c_str());
      if ((*state.senderLogPos)->isOutput() &&
          (*state.senderLogPos)->getParticipant() == SPA::ParticipantName) {
        bool outputSent = false;
        for (auto it : state.symbolics) {
          if (it.second->name == (*state.senderLogPos)->getFullName()) {
            klee_message("[spa_seed_symbol]      Output %s already sent.",
                         (*state.senderLogPos)->getFullName().c_str());
            outputSent = true;
            break;
          }
        }
        if (!outputSent) {
          klee_message("[spa_seed_symbol]   Trying to receive %s before "
                       "sending %s.",
                       fullName.c_str(),
                       (*state.senderLogPos)->getFullName().c_str());
          executor.terminateStateOnError(
              state,
              "Seeding with incompatible path (used symbol out of order).",
              "user.err");
          return;
        }
      }

      if ((*state.senderLogPos)->getQualifiedName() ==
          SPA::seedSymbolMappings[qualifiedName]) {
        break;
      }
    }
  } else if (SPA::connectSockets) {
    if (fullName.compare(0, strlen(SPA_MESSAGE_INPUT_PREFIX),
                         SPA_MESSAGE_INPUT_PREFIX) == 0) {
      // Socket mapping in=out.
      // Skip log entries until named symbol.
      for (; state.senderLogPos != state.senderPath->getSymbolLog().end();
           state.senderLogPos++) {
        klee_message("[spa_seed_symbol]   Checking log entry %d: %s",
                     (int)(state.senderLogPos -
                           state.senderPath->getSymbolLog().begin()),
                     (*state.senderLogPos)->getFullName().c_str());

        // Check for outstanding outputs in log.
        if ((*state.senderLogPos)->isOutput() &&
            (*state.senderLogPos)->getParticipant() == SPA::ParticipantName) {
          bool outputSent = false;
          for (auto it : state.symbolics) {
            if (it.second->name == (*state.senderLogPos)->getFullName()) {
              klee_message("[spa_seed_symbol]      Output %s already sent.",
                           (*state.senderLogPos)->getFullName().c_str());
              outputSent = true;
              break;
            }
          }
          if (!outputSent) {
            klee_message("[spa_seed_symbol]   Trying to receive %s before "
                         "sending %s.",
                         fullName.c_str(),
                         (*state.senderLogPos)->getFullName().c_str());
            executor.terminateStateOnError(
                state,
                "Seeding with incompatible path (used symbol out of order).",
                "user.err");
            return;
          }
        }

        if ((*state.senderLogPos)->isOutput() &&
            (*state.senderLogPos)->isMessage() &&
            checkMessageCompatibility(*state.senderLogPos, localName)) {
          break;
        }
      }
    } else if (fullName.compare(0, strlen(SPA_API_INPUT_PREFIX),
                                SPA_API_INPUT_PREFIX) == 0 ||
               fullName.compare(0, strlen(SPA_MODEL_INPUT_PREFIX),
                                SPA_MODEL_INPUT_PREFIX) == 0) {
      // Check if the current participant hasn't contributed to the sender path.
      // If not, then path is equivalent to the root path from this participants
      // point of view and can't be connected.
      bool participantFound = false;
      for (auto pit : state.senderPath->getParticipants()) {
        if (pit->getName() == SPA::ParticipantName) {
          participantFound = true;
          break;
        }
      }
      if (!participantFound) {
        klee_message(
            "[spa_seed_symbol] Input %s is not connected. Not seeding.",
            fullName.c_str());
        return;
      }

      // API/Model mapping in=in.
      // Skip log entries until same symbol.
      for (; state.senderLogPos != state.senderPath->getSymbolLog().end();
           state.senderLogPos++) {
        klee_message("[spa_seed_symbol]   Checking log entry %d: %s",
                     (int)(state.senderLogPos -
                           state.senderPath->getSymbolLog().begin()),
                     (*state.senderLogPos)->getFullName().c_str());

        // Check for outstanding outputs in log.
        if ((*state.senderLogPos)->isOutput() &&
            (*state.senderLogPos)->getParticipant() == SPA::ParticipantName) {
          bool outputSent = false;
          for (auto it : state.symbolics) {
            if (it.second->name == (*state.senderLogPos)->getFullName()) {
              klee_message("[spa_seed_symbol]      Output %s already sent.",
                           (*state.senderLogPos)->getFullName().c_str());
              outputSent = true;
              break;
            }
          }
          if (!outputSent) {
            klee_message("[spa_seed_symbol]   Trying to receive %s before "
                         "sending %s.",
                         fullName.c_str(),
                         (*state.senderLogPos)->getFullName().c_str());
            executor.terminateStateOnError(
                state,
                "Seeding with incompatible path (used symbol out of order).",
                "user.err");
            return;
          }
        }

        if (fullName == (*state.senderLogPos)->getFullName()) {
          break;
        }
      }
    }
  } else { // Not mapped, leave symbol unconstrained.
    klee_message("[spa_seed_symbol] Input %s is not connected. Not seeding.",
                 fullName.c_str());
    return;
  }

  // Check if log ran out.
  if (state.senderLogPos == state.senderPath->getSymbolLog().end()) {
    executor.terminateStateOnError(
        state, "Seeding with incompatible path (used symbol not yet present).",
        "user.err");
    return;
  }

  if ((*state.senderLogPos)->isOutput()) { // Check if sender symbol is output.
    klee_message("[spa_seed_symbol]   Connecting symbols %s[%ld] = %s[%ld]",
                 fullName.c_str(), size,
                 (*state.senderLogPos)->getFullName().c_str(),
                 (*state.senderLogPos)->getOutputValues().size());

    assert(size >= (*state.senderLogPos)->getOutputValues().size() &&
           "Symbol size mismatch.");
    // Add sender output values = server input array constraint.
    for (size_t offset = 0;
         offset < (*state.senderLogPos)->getOutputValues().size(); offset++) {
      klee::ref<klee::Expr> e = klee::createDefaultExprBuilder()->Eq(
          os->read8(offset), (*state.senderLogPos)->getOutputValues()[offset]);
      if (!state.addAndCheckConstraint(e)) {
        executor.terminateStateOnError(
            state, "Seeding symbol made constraints trivially UNSAT "
                   "(incompatible message for this path).",
            "user.err");
        return;
      }
    }
    // Populate symbol with output expression.
    for (size_t offset = 0;
         offset < (*state.senderLogPos)->getOutputValues().size(); offset++) {
      os->write(offset, (*state.senderLogPos)->getOutputValues()[offset]);
    }
  } else if ((*state.senderLogPos)
                 ->isInput()) { // Check if defined as an input instead.
    klee_message("[spa_seed_symbol]   Connecting symbols %s[%ld] = %s[%d]",
                 fullName.c_str(), size,
                 (*state.senderLogPos)->getFullName().c_str(),
                 (*state.senderLogPos)->getInputArray()->size);

    assert(size == (*state.senderLogPos)->getInputArray()->size &&
           "Symbol size mismatch.");
    // Add sender input values = server input array constraint.
    for (size_t offset = 0;
         offset < (*state.senderLogPos)->getInputArray()->size; offset++) {
      klee::UpdateList ul((*state.senderLogPos)->getInputArray(), 0);
      llvm::OwningPtr<klee::ExprBuilder> exprBuilder(
          klee::createDefaultExprBuilder());
      klee::ref<klee::Expr> e = exprBuilder->Eq(
          os->read8(offset),
          exprBuilder->Read(ul,
                            exprBuilder->Constant(offset, klee::Expr::Int32)));
      if (!state.addAndCheckConstraint(e)) {
        executor.terminateStateOnError(
            state, "Seeding symbol made constraints trivially UNSAT "
                   "(incompatible message for this path).",
            "user.err");
        return;
      }
    }
  } else {
    assert(false && "Sender symbol neither input nor output.");
  }
  // Consume Log entry.
  state.senderLogPos++;
}

void
SpecialFunctionHandler::handleSpaSnprintf5(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 8 &&
         "Invalid number of arguments to spa_snprintf5.");

  klee::ConstantExpr *ce;
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[1])));
  unsigned length = ce->getZExtValue();

  std::string format = readStringAtAddress(state, arguments[2]);
  std::string in1 = readStringAtAddress(state, arguments[3]);
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[4])));
  unsigned long in2 = ce->getZExtValue();
  std::string in3 = readStringAtAddress(state, arguments[5]);
  std::string in4 = readStringAtAddress(state, arguments[6]);
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[7])));
  unsigned long in5 = ce->getZExtValue();

  char *result = new char[length];
  unsigned resultSize = snprintf(result, length, format.c_str(), in1.c_str(),
                                 in2, in3.c_str(), in4.c_str(), in5) + 1;

  ObjectPair op;
  ref<ConstantExpr> address =
      cast<ConstantExpr>(executor.toUnique(state, arguments[0]));
  assert(state.addressSpace.resolveOne(address, op) &&
         "XXX out of bounds / multiple resolution unhandled");
  bool res;
  assert(executor.solver->mustBeTrue(
      state, EqExpr::create(address, op.first->getBaseExpr()), res) && res &&
         "XXX interior pointer unhandled");
  ObjectState *os = const_cast<ObjectState *>(op.second);

  unsigned i;
  for (i = 0; i <= resultSize; i++) {
    os->write8(i, result[i]);
  }
  delete[] result;
}

void SpecialFunctionHandler::handleSpaRuntimeCall(
    ExecutionState &state, KInstruction *target,
    std::vector<ref<Expr> > &arguments) {}

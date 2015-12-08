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
  add("spa_seed_symbol_check", handleSpaSeedSymbolCheck, true),
  add("spa_seed_symbol", handleSpaSeedSymbol, false),
  add("spa_runtime_call", handleSpaRuntimeCall, false),
  add("spa_snprintf3", handleSpaSnprintf3, false),
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

PathLoader *senderPaths = NULL;
bool followSenderPaths = false;
std::map<std::string, std::string> seedSymbolMappings;
bool connectSockets = false;
}

void SpecialFunctionHandler::handleSpaSeedSymbolCheck(
    ExecutionState &state, KInstruction *target,
    std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 1 &&
         "Invalid number of arguments to spa_seed_symbol_check.");

  klee::ConstantExpr *ce;
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[0])));
  uint64_t pathID = ce->getZExtValue();

  if (!SPA::senderPaths) {
    klee_message(
        "[spa_seed_symbol_check] No sender path-file. Not seeding symbol.");
    assert(pathID == 0 &&
           "Checking for non-zero pathID without sender path-file.");
    executor.bindLocal(target, state, ConstantExpr::create(false, Expr::Int32));
  } else if (SPA::followSenderPaths) {
    klee_message(
        "[spa_seed_symbol_check] Following sender path-file. Allowing symbol "
        "to be seeded with path %ld. Path may not be ready yet.",
        pathID);
    executor.bindLocal(target, state, ConstantExpr::create(true, Expr::Int32));
  } else {
    if (SPA::senderPaths->getPath(pathID)) {
      klee_message("[spa_seed_symbol_check] Finite sender path-file. Allowing "
                   "symbol to be seeded with path %ld.",
                   pathID);
      executor.bindLocal(target, state,
                         ConstantExpr::create(true, Expr::Int32));
    } else {
      klee_message("[spa_seed_symbol_check] Finished processing sender "
                   "path-file. Path %ld out-of-bound. Terminating.",
                   pathID);
      executor.terminateStateOnError(state, "Path ID out-of-bounds.",
                                     "user.err");
    }
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
    klee_message("No sender path-file specified. Not seeding symbol.");
    assert(pathID == 0 &&
           "Seeding non-zero pathID without a sender path-file.");
    return;
  }

  if (!state.senderPath) {
    klee_message("  Loading path %ld from file.", pathID);
    while (!(state.senderPath.reset(SPA::senderPaths->getPath(pathID)),
             state.senderPath)) {
      klee_message("    Path %ld not yet available. Waiting.", pathID);
      sleep(1);
    }
    assert(state.senderPath &&
           "Attempting to process seed path before it's available.");

    klee_message("  Starting joint symbolic execution on path %s.",
                 state.senderPath->getUUID().c_str());

    state.senderLogPos = state.senderPath->getSymbolLog().begin();

    // Get current participant's IP/Port.
    std::string participantIPPort = "0.0.0.0.0";
    for (auto it : state.senderPath->getOutputSymbols()) {
      std::string name = it.first;
      std::string participantName =
          name.substr(name.rfind(SPA_SYMBOL_DELIMITER) + 1);
      if (name.compare(0, strlen(SPA_MESSAGE_OUTPUT_SOURCE_PREFIX),
                       SPA_MESSAGE_OUTPUT_SOURCE_PREFIX) == 0 &&
          participantName == SPA::ParticipantName) {
        struct sockaddr_in src;
        assert(it.second[0]->getOutputValues().size() == sizeof(src));
        for (unsigned i = 0; i < sizeof(src); i++) {
          ConstantExpr *ce =
              llvm::dyn_cast<ConstantExpr>(it.second[0]->getOutputValues()[i]);
          assert(ce && "Non-constant source IP.");
          ((char *)&src)[i] = ce->getLimitedValue();
        }
        char srcTxt[INET_ADDRSTRLEN];
        assert(inet_ntop(AF_INET, &src.sin_addr, srcTxt, sizeof(srcTxt)));
        participantIPPort =
            std::string(srcTxt) + "." + SPA::numToStr(ntohs(src.sin_port));
        break;
      }
    }
    // Check if there are any symbols in the log that can be consumed but that
    // come after any symbols outputted by the current participant.
    // To check this, check the log in reverse and find the first of either a
    // symbol outputted by the current participant or a symbol that can be
    // consumed.
    for (auto it = state.senderPath->getSymbolLog().rbegin();
         it != state.senderPath->getSymbolLog().rend(); it++) {
      if ((*it)->isOutput()) {
        std::string symbolName = (*it)->getName();
        std::string symbolQualifiedName =
            symbolName.substr(0, symbolName.rfind(SPA_SYMBOL_DELIMITER));
        std::string symbolParticipant = symbolQualifiedName.substr(
            symbolQualifiedName.rfind(SPA_SYMBOL_DELIMITER) + 1);
        std::string symbolLocalName = symbolQualifiedName.substr(
            0, symbolQualifiedName.rfind(SPA_SYMBOL_DELIMITER));
        std::string symbolIPPort = symbolLocalName.substr(
            symbolLocalName.rfind(SPA_SYMBOL_DELIMITER) + 1);

        // Check if symbol can be consumed by a direct mapping.
        if (SPA::seedSymbolMappings.count(symbolQualifiedName)) {
          break;
        }
        // Check if symbol can be consumed via symbolic socket layer.
        if (SPA::connectSockets && symbolIPPort == participantIPPort) {
          break;
        }
        // Check if symbol was outputted by current participant.
        if (symbolParticipant == SPA::ParticipantName) {
          klee_message("[spa_seed_symbol] Cannot seed path with no new inputs."
                       "Terminating.");
          executor.terminateStateOnError(state, "Path has no new inputs.",
                                         "user.err");
          return;
        }
      }
    }

    // Add client path constraints.
    klee_message("  ANDing in sender path constraints.");
    for (auto it : state.senderPath->getConstraints()) {
      // Don't add internal symbols as they may collide.
      if (klee::EqExpr *eqExpr = llvm::dyn_cast<klee::EqExpr>(it)) {
        if (klee::ConcatExpr *catExpr =
                llvm::dyn_cast<klee::ConcatExpr>(eqExpr->right)) {
          if (klee::ReadExpr *rdExpr =
                  llvm::dyn_cast<klee::ReadExpr>(catExpr->getLeft())) {
            if (rdExpr->updates.root->name.compare(
                    0, strlen(SPA_INTERNAL_PREFIX), SPA_INTERNAL_PREFIX) == 0) {
              continue;
            }
          }
        }
      }
      if (!state.addAndCheckConstraint(it)) {
        executor.terminateStateOnError(
            state, "Seeding symbol made constraints trivially UNSAT "
                   "(incompatible message for this path).",
            "user.err");
        return;
      }
    }
  }

  Executor::ExactResolutionList rl;
  executor.resolveExact(state, arguments[0], rl, "seed_symbol");
  assert(rl.size() == 1 &&
         "Seeding symbol that doesn't resolve to a single object.");
  const MemoryObject *mo = rl[0].first.first;
  const ObjectState *os = rl[0].first.second;
  assert(!os->readOnly && "Seeding read-only object.");
  std::string fullName = mo->name;
  size_t size = mo->getSizeExpr()->getZExtValue(sizeof(size_t) * 8);

  klee_message("Seeding path %ld, on input: %s", pathID, fullName.c_str());

  // Drop the participant and sequence number.
  std::string baseName =
      fullName.substr(0, fullName.rfind(SPA_SYMBOL_DELIMITER));
  baseName = baseName.substr(0, baseName.rfind(SPA_SYMBOL_DELIMITER));

  // Check if input mapped.
  if (SPA::seedSymbolMappings.count(baseName)) { // Explicit mapping.
    std::string senderName = SPA::seedSymbolMappings[baseName];
    // Skip log entries until named symbol.
    for (; state.senderLogPos != state.senderPath->getSymbolLog().end();
         state.senderLogPos++) {
      std::string symbolName = (*state.senderLogPos)->getName();
      // Strip participant and sequence number from sender symbol as well.
      symbolName = symbolName.substr(0, symbolName.rfind(SPA_SYMBOL_DELIMITER));
      symbolName = symbolName.substr(0, symbolName.rfind(SPA_SYMBOL_DELIMITER));
      if (symbolName == senderName) {
        break;
      }
    }
  } else if (SPA::connectSockets) {
    if (baseName.compare(0, strlen(SPA_MESSAGE_INPUT_PREFIX),
                         SPA_MESSAGE_INPUT_PREFIX) == 0) {
      // Socket mapping in=out.
      // Convert symbol name from in to out.
      std::string senderPrefix =
          std::string(SPA_MESSAGE_OUTPUT_PREFIX) +
          baseName.substr(strlen(SPA_MESSAGE_INPUT_PREFIX)) + "_";
      // Skip log entries until named symbol.
      while (state.senderLogPos != state.senderPath->getSymbolLog().end() &&
             (*state.senderLogPos)->getName().compare(0, senderPrefix.length(),
                                                      senderPrefix) != 0) {
        state.senderLogPos++;
      }
    } else if (baseName.compare(0, strlen(SPA_API_INPUT_PREFIX),
                                SPA_API_INPUT_PREFIX) == 0) {
      // The root path can't be connected.
      if (pathID == 0) {
        klee_message("Input %s is not connected. Not seeding.",
                     fullName.c_str());
        return;
      }

      // API mapping in=in.
      // Skip log entries until same symbol.
      while (state.senderLogPos != state.senderPath->getSymbolLog().end() &&
             fullName != (*state.senderLogPos)->getName()) {
        state.senderLogPos++;
      }
    }
  } else { // Not mapped, leave symbol unconstrained.
    klee_message("Input %s is not connected. Not seeding.", fullName.c_str());
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
    klee_message("  Connecting symbols %s[%ld] = %s[%ld]", fullName.c_str(),
                 size, (*state.senderLogPos)->getName().c_str(),
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
  } else if ((*state.senderLogPos)
                 ->isInput()) { // Check if defined as an input instead.
    klee_message("  Connecting symbols %s[%ld] = %s[%d]", fullName.c_str(),
                 size, (*state.senderLogPos)->getName().c_str(),
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
}

void
SpecialFunctionHandler::handleSpaSnprintf3(ExecutionState &state,
                                           KInstruction *target,
                                           std::vector<ref<Expr> > &arguments) {
  assert(arguments.size() == 6 &&
         "Invalid number of arguments to spa_sprintf3.");

  klee::ConstantExpr *ce;
  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[1])));
  unsigned length = ce->getZExtValue();

  std::string format = readStringAtAddress(state, arguments[2]);
  std::string in1 = readStringAtAddress(state, arguments[3]);
  std::string in2 = readStringAtAddress(state, arguments[4]);

  assert(ce = dyn_cast<ConstantExpr>(executor.toUnique(state, arguments[5])));
  unsigned long in3 = ce->getZExtValue();

  char *result = new char[length];
  unsigned resultSize = snprintf(result, length, format.c_str(), in1.c_str(),
                                 in2.c_str(), in3) + 1;

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

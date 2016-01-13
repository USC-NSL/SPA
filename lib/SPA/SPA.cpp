/*
 * SPA - Systematic Protocol Analysis Framework
 */

#include <list>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <vector>
#include <map>
#include <set>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cassert>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/system_error.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"

#include "klee/Internal/System/Time.h"
#include "klee/Internal/Support/ModuleUtil.h"
#include "../Core/Common.h"
#include "../Core/Executor.h"
#include "../Core/Searcher.h"
#include "klee/Expr.h"
#include "klee/ExprBuilder.h"
#include <expr/Parser.h>
#include "../../lib/Core/Context.h"
#include "../../lib/Core/TimingSolver.h"
#include "klee/Statistics.h"

#include <spa/SPA.h>
#include <spa/SpaSearcher.h>
#include <spa/Path.h>
#include <spa/Util.h>

#define DUMP_COVERAGE_PERIOD 10000

#define MAIN_ENTRY_FUNCTION "__user_main"
#define ALTERNATIVE_MAIN_ENTRY_FUNCTION "main"
#define OLD_ENTRY_FUNCTION "__spa_old_main"
#define KLEE_INT_FUNCTION "klee_int"
#define MALLOC_FUNCTION "malloc"

namespace {
typedef enum {
  START,
  PATH,
  SYMBOL_NAME,
  SYMBOL_ARRAY,
  SYMBOL_VALUE,
  SYMBOL_END,
  TAG_KEY,
  TAG_VALUE,
  CONSTRAINTS,
  PATH_DONE
} LoadState_t;
}

namespace SPA {
extern llvm::cl::opt<std::string> ParticipantName;

llvm::cl::opt<std::string>
    IP("ip", llvm::cl::desc("Sets the participant IP address when bind doesn't "
                            "(default: 127.0.0.1)."));

llvm::cl::opt<int>
    MaxPaths("max-paths",
             llvm::cl::desc("Stop after outputting this many paths."));

llvm::cl::opt<bool> StepDebug(
    "step-debug",
    llvm::cl::desc("Enables outputting debug data at each execution step."));

llvm::cl::opt<std::string> RecoverState(
    "recover-state",
    llvm::cl::desc(
        "Specifies a file with a previously saved processing queue to load."));

llvm::cl::opt<std::string> DumpCov(
    "dump-cov",
    llvm::cl::desc(
        "Specifies a file to periodically dump global coverage data to."));

extern PathLoader *senderPaths;
extern bool followSenderPaths;
extern std::map<std::string, std::string> seedSymbolMappings;
extern bool connectSockets;

std::set<llvm::Instruction *> coverage;

static void interrupt_handle() {
  std::cerr << "SPA: Ctrl-C detected, exiting.\n";
  exit(1);
}

// This is a terrible hack until we get some real modeling of the
// system. All we do is check the undefined symbols and warn about
// any "unrecognized" externals and about any obviously unsafe ones.

// Symbols we explicitly support
static const char *modelledExternals[] = {
  "_ZTVN10__cxxabiv117__class_type_infoE",
  "_ZTVN10__cxxabiv120__si_class_type_infoE",
  "_ZTVN10__cxxabiv121__vmi_class_type_infoE",

  // special functions
  "_assert", "__assert_fail", "__assert_rtn", "calloc", "_exit", "exit", "free",
  "abort", "klee_abort", "klee_assume", "klee_check_memory_access",
  "klee_define_fixed_object", "klee_get_errno", "klee_get_valuef",
  "klee_get_valued", "klee_get_valuel", "klee_get_valuell",
  "klee_get_value_i32", "klee_get_value_i64", "klee_get_obj_size",
  "klee_is_symbolic", "klee_make_symbolic", "klee_mark_global", "klee_merge",
  "klee_prefer_cex", "klee_print_expr", "klee_print_range", "klee_report_error",
  "klee_set_forking", "klee_silent_exit", "klee_warning", "klee_warning_once",
  "klee_alias_function", "klee_stack_trace",
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 1)
  "llvm.dbg.declare", "llvm.dbg.value",
#endif
  "llvm.va_start", "llvm.va_end", "malloc", "realloc", "_ZdaPv", "_ZdlPv",
  "_Znaj", "_Znwj", "_Znam", "_Znwm",
};
// Symbols we aren't going to warn about
static const char *dontCareExternals[] = {
#if 0
  // stdio
  "fprintf", "fflush", "fopen", "fclose", "fputs_unlocked", "putchar_unlocked",
  "vfprintf", "fwrite", "puts", "printf", "stdin", "stdout", "stderr",
  "_stdio_term", "__errno_location", "fstat",
#endif

  // static information, pretty ok to return
  "getegid", "geteuid", "getgid", "getuid", "getpid", "gethostname", "getpgrp",
  "getppid", "getpagesize", "getpriority", "getgroups", "getdtablesize",
  "getrlimit", "getrlimit64", "getcwd", "getwd", "gettimeofday", "uname",

  // fp stuff we just don't worry about yet
  "frexp", "ldexp", "__isnan", "__signbit",
};
// 	// Extra symbols we aren't going to warn about with klee-libc
// 	static const char *dontCareKlee[] = {
// 		"__ctype_b_loc",
// 		"__ctype_get_mb_cur_max",
//
// 		// io system calls
// 		"open",
// 		"write",
// 		"read",
// 		"close",
// 	};
// Extra symbols we aren't going to warn about with uclibc
static const char *dontCareUclibc[] = {
  "__dso_handle",

  // Don't warn about these since we explicitly commented them out of
  // uclibc.
  "printf", "vprintf"
};
// Symbols we consider unsafe
static const char *unsafeExternals[] = { "fork",  // oh lord
                                         "exec",  // heaven help us
                                         "error", // calls _exit
                                         "raise", // yeah
                                         "kill",  // mmmhmmm
};
#define NELEMS(array) (sizeof(array) / sizeof(array[0]))
void externalsAndGlobalsCheck(const llvm::Module *m) {
  std::map<std::string, bool> externals;
  std::set<std::string> modelled(modelledExternals,
                                 modelledExternals + NELEMS(modelledExternals));
  std::set<std::string> dontCare(dontCareExternals,
                                 dontCareExternals + NELEMS(dontCareExternals));
  std::set<std::string> unsafe(unsafeExternals,
                               unsafeExternals + NELEMS(unsafeExternals));

  // Assume using KLEE uClibc
  dontCare.insert(dontCareUclibc, dontCareUclibc + NELEMS(dontCareUclibc));
  // Assume using POSIX runtime.
  dontCare.insert("syscall");

  for (llvm::Module::const_iterator fnIt = m->begin(), fn_ie = m->end();
       fnIt != fn_ie; ++fnIt) {
    if (fnIt->isDeclaration() && !fnIt->use_empty())
      externals.insert(std::make_pair(fnIt->getName(), false));
    for (llvm::Function::const_iterator bbIt = fnIt->begin(),
                                        bb_ie = fnIt->end();
         bbIt != bb_ie; ++bbIt) {
      for (llvm::BasicBlock::const_iterator it = bbIt->begin(),
                                            ie = bbIt->end();
           it != ie; ++it) {
        if (const llvm::CallInst *ci = dyn_cast<llvm::CallInst>(it)) {
          if (isa<llvm::InlineAsm>(ci->getCalledValue())) {
            klee::klee_warning_once(&*fnIt, "function \"%s\" has inline asm",
                                    fnIt->getName().data());
          }
        }
      }
    }
  }
  for (llvm::Module::const_global_iterator it = m->global_begin(),
                                           ie = m->global_end();
       it != ie; ++it)
    if (it->isDeclaration() && !it->use_empty())
      externals.insert(std::make_pair(it->getName(), true));
  // and remove aliases (they define the symbol after global
  // initialization)
  for (llvm::Module::const_alias_iterator it = m->alias_begin(),
                                          ie = m->alias_end();
       it != ie; ++it) {
    std::map<std::string, bool>::iterator it2 = externals.find(it->getName());
    if (it2 != externals.end())
      externals.erase(it2);
  }

  std::map<std::string, bool> foundUnsafe;
  for (std::map<std::string, bool>::iterator it = externals.begin(),
                                             ie = externals.end();
       it != ie; ++it) {
    const std::string &ext = it->first;
    if (!modelled.count(ext) && !dontCare.count(ext)) {
      if (unsafe.count(ext)) {
        foundUnsafe.insert(*it);
      } else {
        klee::klee_warning("undefined reference to %s: %s",
                           it->second ? "variable" : "function", ext.c_str());
      }
    }
  }

  for (std::map<std::string, bool>::iterator it = foundUnsafe.begin(),
                                             ie = foundUnsafe.end();
       it != ie; ++it) {
    const std::string &ext = it->first;
    klee::klee_warning("undefined reference to %s: %s (UNSAFE)!",
                       it->second ? "variable" : "function", ext.c_str());
  }
}

static int initEnv(llvm::Module *mainModule) {
  /*
		 *    nArgcP = alloc oldArgc->getType()
		 *    nArgvV = alloc oldArgv->getType()
		 *    store oldArgc nArgcP
		 *    store oldArgv nArgvP
		 *    klee_init_environment(nArgcP, nArgvP)
		 *    nArgc = load nArgcP
		 *    nArgv = load nArgvP
		 *    oldArgc->replaceAllUsesWith(nArgc)
		 *    oldArgv->replaceAllUsesWith(nArgv)
		 */
  llvm::Function *mainFn = mainModule->getFunction("main");

  if (mainFn->arg_size() < 2) {
    klee::klee_error("Cannot handle "
                     "--posix-runtime"
                     " when main() has less than two arguments.\n");
  }

  llvm::Instruction *firstInst = mainFn->begin()->begin();

  llvm::Value *oldArgc = mainFn->arg_begin();
  llvm::Value *oldArgv = ++mainFn->arg_begin();

  llvm::AllocaInst *argcPtr =
      new llvm::AllocaInst(oldArgc->getType(), "argcPtr", firstInst);
  llvm::AllocaInst *argvPtr =
      new llvm::AllocaInst(oldArgv->getType(), "argvPtr", firstInst);

  /* Insert void klee_init_env(int* argc, char*** argv) */
  std::vector<const llvm::Type *> params;
  params.push_back(llvm::Type::getInt32Ty(llvm::getGlobalContext()));
  params.push_back(llvm::Type::getInt32Ty(llvm::getGlobalContext()));
  llvm::Function *initEnvFn =
      cast<llvm::Function>(mainModule->getOrInsertFunction(
          "klee_init_env", llvm::Type::getVoidTy(llvm::getGlobalContext()),
          argcPtr->getType(), argvPtr->getType(), NULL));
  assert(initEnvFn);
  std::vector<llvm::Value *> args;
  args.push_back(argcPtr);
  args.push_back(argvPtr);
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 0)
  llvm::Instruction *initEnvCall =
      llvm::CallInst::Create(initEnvFn, args, "", firstInst);
#else
  llvm::Instruction *initEnvCall = llvm::CallInst::Create(
      initEnvFn, args.begin(), args.end(), "", firstInst);
#endif
  llvm::Value *argc = new llvm::LoadInst(argcPtr, "newArgc", firstInst);
  llvm::Value *argv = new llvm::LoadInst(argvPtr, "newArgv", firstInst);

  oldArgc->replaceAllUsesWith(argc);
  oldArgv->replaceAllUsesWith(argv);

  new llvm::StoreInst(oldArgc, argcPtr, initEnvCall);
  new llvm::StoreInst(oldArgv, argvPtr, initEnvCall);

  return 0;
}

#ifndef SUPPORT_KLEE_UCLIBC
static llvm::Module *linkWithUclibc(llvm::Module *mainModule,
                                    llvm::sys::Path libDir) {
  fprintf(stderr, "error: invalid libc, no uclibc support!\n");
  exit(1);
  return 0;
}
#else
static void replaceOrRenameFunction(llvm::Module *module, const char *old_name,
                                    const char *new_name) {
  llvm::Function *f, *f2;
  f = module->getFunction(new_name);
  f2 = module->getFunction(old_name);
  if (f2) {
    if (f) {
      f2->replaceAllUsesWith(f);
      f2->eraseFromParent();
    } else {
      f2->setName(new_name);
      assert(f2->getName() == new_name);
    }
  }
}

static llvm::Module *linkWithUclibc(llvm::Module *mainModule,
                                    llvm::sys::Path libDir) {
  // Ensure that klee-uclibc exists
  llvm::sys::Path uclibcBCA(libDir);
  uclibcBCA.appendComponent(KLEE_UCLIBC_BCA_NAME);

  bool uclibcExists = false;
  llvm::sys::fs::exists(uclibcBCA.c_str(), uclibcExists);
  if (!uclibcExists)
    klee::klee_error("Cannot find klee-uclibc : %s", uclibcBCA.c_str());

  // force import of __uClibc_main
  mainModule->getOrInsertFunction(
      "__uClibc_main",
      llvm::FunctionType::get(llvm::Type::getVoidTy(llvm::getGlobalContext()),
                              std::vector<LLVM_TYPE_Q llvm::Type *>(), true));

  // force various imports
  LLVM_TYPE_Q llvm::Type *i8Ty =
      llvm::Type::getInt8Ty(llvm::getGlobalContext());
  mainModule->getOrInsertFunction(
      "realpath", llvm::PointerType::getUnqual(i8Ty),
      llvm::PointerType::getUnqual(i8Ty), llvm::PointerType::getUnqual(i8Ty),
      NULL);
  mainModule->getOrInsertFunction("getutent",
                                  llvm::PointerType::getUnqual(i8Ty), NULL);
  mainModule->getOrInsertFunction(
      "__fgetc_unlocked", llvm::Type::getInt32Ty(llvm::getGlobalContext()),
      llvm::PointerType::getUnqual(i8Ty), NULL);
  mainModule->getOrInsertFunction(
      "__fputc_unlocked", llvm::Type::getInt32Ty(llvm::getGlobalContext()),
      llvm::Type::getInt32Ty(llvm::getGlobalContext()),
      llvm::PointerType::getUnqual(i8Ty), NULL);

  llvm::Function *f = mainModule->getFunction("__ctype_get_mb_cur_max");
  if (f)
    f->setName("_stdlib_mb_cur_max");

  // Strip off asm prefixes for 64 bit versions because they are not
  // present in uclibc and we want to make sure stuff will get
  // linked. In the off chance that both prefixed and unprefixed
  // versions are present in the module, make sure we don't create a
  // naming conflict.
  for (llvm::Module::iterator fi = mainModule->begin(), fe = mainModule->end();
       fi != fe; ++fi) {
    llvm::Function *f = fi;
    const std::string &name = f->getName();
    if (name[0] == '\01') {
      unsigned size = name.size();
      if (name[size - 2] == '6' && name[size - 1] == '4') {
        std::string unprefixed = name.substr(1);

        // See if the unprefixed version exists.
        if (llvm::Function *f2 = mainModule->getFunction(unprefixed)) {
          f->replaceAllUsesWith(f2);
          f->eraseFromParent();
        } else {
          f->setName(unprefixed);
        }
      }
    }
  }

  mainModule = klee::linkWithLibrary(mainModule, uclibcBCA.c_str());
  assert(mainModule && "unable to link with uclibc");

  replaceOrRenameFunction(mainModule, "__libc_open", "open");
  replaceOrRenameFunction(mainModule, "__libc_fcntl", "fcntl");

  // XXX we need to rearchitect so this can also be used with
  // programs externally linked with uclibc.

  // We now need to swap things so that __uClibc_main is the entry
  // point, in such a way that the arguments are passed to
  // __uClibc_main correctly. We do this by renaming the user main
  // and generating a stub function to call __uClibc_main. There is
  // also an implicit cooperation in that runFunctionAsMain sets up
  // the environment arguments to what uclibc expects (following
  // argv), since it does not explicitly take an envp argument.
  llvm::Function *userMainFn = mainModule->getFunction("main");
  assert(userMainFn && "unable to get user main");
  llvm::Function *uclibcMainFn = mainModule->getFunction("__uClibc_main");
  assert(uclibcMainFn && "unable to get uclibc main");
  userMainFn->setName("__user_main");

  const llvm::FunctionType *ft = uclibcMainFn->getFunctionType();
  assert(ft->getNumParams() == 7);

  std::vector<LLVM_TYPE_Q llvm::Type *> fArgs;
  fArgs.push_back(ft->getParamType(1)); // argc
  fArgs.push_back(ft->getParamType(2)); // argv
  llvm::Function *stub = llvm::Function::Create(
      llvm::FunctionType::get(llvm::Type::getInt32Ty(llvm::getGlobalContext()),
                              fArgs, false),
      llvm::GlobalVariable::ExternalLinkage, "main", mainModule);
  llvm::BasicBlock *bb =
      llvm::BasicBlock::Create(llvm::getGlobalContext(), "entry", stub);

  std::vector<llvm::Value *> args;
  args.push_back(
      llvm::ConstantExpr::getBitCast(userMainFn, ft->getParamType(0)));
  args.push_back(stub->arg_begin());                                 // argc
  args.push_back(++stub->arg_begin());                               // argv
  args.push_back(llvm::Constant::getNullValue(ft->getParamType(3))); // app_init
  args.push_back(llvm::Constant::getNullValue(ft->getParamType(4))); // app_fini
  args.push_back(
      llvm::Constant::getNullValue(ft->getParamType(5))); // rtld_fini
  args.push_back(
      llvm::Constant::getNullValue(ft->getParamType(6))); // stack_end
#if LLVM_VERSION_CODE >= LLVM_VERSION(3, 0)
  llvm::CallInst::Create(uclibcMainFn, args, "", bb);
#else
  llvm::CallInst::Create(uclibcMainFn, args.begin(), args.end(), "", bb);
#endif

  new llvm::UnreachableInst(llvm::getGlobalContext(), bb);

  klee::klee_message("NOTE: Using klee-uclibc : %s", uclibcBCA.c_str());
  return mainModule;
}
#endif

llvm::Module *SPA::getModuleFromFile(std::string moduleFile) {
  llvm::InitializeNativeTarget();

  // Load the bytecode...
  std::string ErrorMsg;
  llvm::Module *mainModule = 0;
  llvm::OwningPtr<llvm::MemoryBuffer> BufferPtr;
  llvm::error_code ec =
      llvm::MemoryBuffer::getFileOrSTDIN(moduleFile.c_str(), BufferPtr);
  if (ec)
    klee::klee_error("error loading program '%s': %s", moduleFile.c_str(),
                     ec.message().c_str());
  mainModule = getLazyBitcodeModule(BufferPtr.take(), llvm::getGlobalContext(),
                                    &ErrorMsg);

  if (mainModule) {
    if (mainModule->MaterializeAllPermanently(&ErrorMsg)) {
      delete mainModule;
      mainModule = 0;
    }
  }
  if (!mainModule)
    klee::klee_error("error loading program '%s': %s", moduleFile.c_str(),
                     ErrorMsg.c_str());

  assert(initEnv(mainModule) == 0 && "Unable to initialize POSIX environment.");

  llvm::sys::Path LibraryDir(KLEE_DIR "/" RUNTIME_CONFIGURATION "/lib");
  mainModule = linkWithUclibc(mainModule, LibraryDir);

  llvm::sys::Path modelFile = LibraryDir;
  modelFile.appendComponent("libkleeRuntimePOSIX.bca");
  klee::klee_message("NOTE: Using model: %s", modelFile.c_str());
  mainModule = klee::linkWithLibrary(mainModule, modelFile.c_str());
  assert(mainModule && "Unable to link with simple model.");

  // Replace klee-uClibc functions with spa model.
  replaceOrRenameFunction(mainModule, "socket", "spa_socket");
  replaceOrRenameFunction(mainModule, "bind", "spa_bind");
  replaceOrRenameFunction(mainModule, "listen", "spa_listen");

  return mainModule;
}

SPA::SPA(llvm::Module *_module, std::ostream &_output)
    : module(_module), output(_output), pathFilter(NULL),
      outputTerminalPaths(true), checkpointsFound(0), filteredPathsFound(0),
      terminalPathsFound(0), outputtedPaths(0) {
  checkpointFilter.addIF(&checkpointWhitelist);
  stopPointFilter.addIF(&stopPointWhitelist);

#if ENABLE_STPLOG == 1
  STPLOG_init("stplog.c");
#endif

  atexit(llvm::llvm_shutdown); // Call llvm_shutdown() on exit.
  llvm::InitializeNativeTarget();
  llvm::sys::PrintStackTraceOnErrorSignal();
  llvm::sys::SetInterruptFunction(interrupt_handle);

  if (ParticipantName.empty()) {
    ParticipantName = module->getModuleIdentifier();
  }

  generateMain();
}

/**
 * Generates a new main function that looks like:
 *
 * int main( int argc, char **argv ) {
 *   init1();
 *   init2();
 *   (...)
 *   int initValueID = klee_int( "initValueID" );
 *   switch ( initValueID ) {
 *     case 1:
 *       var = malloc( (numValues + 1) * 2 );
 *       var[0] = malloc( values[0].length );
 *       var[0][0] = values[0][0]; (...);
 *       var[1] = malloc( valueMasks[0].length );
 *       var[1][0] = valueMasks[0][0]; (...);
 *       (...);
 *       break;
 *     (...)
 *     default: return 0;
 *   }
 *   int handlerID = klee_int( "handlerID" );
 *   switch ( handlerID ) {
 *     case 1: handler1(); break;
 *     case 2: spa_internal_SeedID = seedID; handler2(); break;
 *     (...)
 *   }
 *   (...)
 *   return 0;
 * }
 */
void SPA::generateMain() {
  // Rename old main function
  llvm::Function *oldEntryFunction = module->getFunction(MAIN_ENTRY_FUNCTION);
  if (!oldEntryFunction)
    oldEntryFunction = module->getFunction(ALTERNATIVE_MAIN_ENTRY_FUNCTION);
  assert(oldEntryFunction && "No main function found to replace.");
  std::string entryFunctionName = oldEntryFunction->getName().str();
  oldEntryFunction->setName(OLD_ENTRY_FUNCTION);
  // Create new one.
  entryFunction = llvm::dyn_cast<llvm::Function>(module->getOrInsertFunction(
      entryFunctionName, oldEntryFunction->getFunctionType()));
  assert(entryFunction && entryFunction->empty() &&
         "Could not add new entry function.");
  entryFunction->setCallingConv(llvm::CallingConv::C);
  // Replace the old with the new.
  oldEntryFunction->replaceAllUsesWith(entryFunction);

  // Declare arguments.
  llvm::Function::arg_iterator args = entryFunction->arg_begin();
  llvm::Value *argcVar = args++;
  argcVar->setName("argc");
  llvm::Value *argvVar = args++;
  argvVar->setName("argv");

  // Create the entry, current and next handler, and return basic blocks.
  llvm::BasicBlock *initBB =
      llvm::BasicBlock::Create(module->getContext(), "", entryFunction, 0);
  firstHandlerBB =
      llvm::BasicBlock::Create(module->getContext(), "", entryFunction, 0);
  nextHandlerBB = firstHandlerBB;
  returnBB =
      llvm::BasicBlock::Create(module->getContext(), "", entryFunction, 0);

  // Allocate arguments.
  new llvm::StoreInst(
      argcVar,
      new llvm::AllocaInst(llvm::IntegerType::get(module->getContext(), 32), "",
                           initBB),
      false, initBB);
  new llvm::StoreInst(
      argvVar, new llvm::AllocaInst(
                   llvm::PointerType::get(
                       llvm::PointerType::get(
                           llvm::IntegerType::get(module->getContext(), 8), 0),
                       0),
                   "", initBB),
      false, initBB);

  // Get klee_int function.
  llvm::Function *kleeIntFunction = module->getFunction(KLEE_INT_FUNCTION);
  if (!kleeIntFunction) {
    kleeIntFunction = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::IntegerType::get(module->getContext(), 32),
            llvm::ArrayRef<llvm::Type *>(llvm::PointerType::get(
                llvm::IntegerType::get(module->getContext(), 8), 0)),
            false),
        llvm::GlobalValue::ExternalLinkage, KLEE_INT_FUNCTION, module);
    kleeIntFunction->setCallingConv(llvm::CallingConv::C);
  }
  // Create initValueID variable.
  llvm::GlobalVariable *initValueIDVarName = new llvm::GlobalVariable(
      *module,
      llvm::ArrayType::get(llvm::IntegerType::get(module->getContext(), 8),
                           strlen(SPA_INITVALUEID_VARIABLE) + 1),
      true, llvm::GlobalValue::PrivateLinkage, NULL, SPA_INITVALUEID_VARIABLE);
  initValueIDVarName->setInitializer(llvm::ConstantDataArray::getString(
      module->getContext(), SPA_INITVALUEID_VARIABLE, true));
  // initValueID = klee_int();
  llvm::Constant *idxsa[] = {
    llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0, true)),
    llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0, true))
  };
  llvm::ArrayRef<llvm::Constant *> idxsar(idxsa, 2);
  llvm::CallInst *kleeIntCall = llvm::CallInst::Create(
      kleeIntFunction,
      llvm::ConstantExpr::getGetElementPtr(initValueIDVarName, idxsar, false),
      "", initBB);
  kleeIntCall->setCallingConv(llvm::CallingConv::C);
  kleeIntCall->setTailCall(false);
  // Init handlers will be later added before this point.
  initHandlerPlaceHolder = kleeIntCall;
  // switch ( handlerID )
  initValueSwitchInst =
      llvm::SwitchInst::Create(kleeIntCall, returnBB, 1, initBB);
  // Entry handlers will be later added starting with id = 1.
  initValueID = 1;

  // Create handlerID variable.
  handlerIDVarName = new llvm::GlobalVariable(
      *module,
      llvm::ArrayType::get(llvm::IntegerType::get(module->getContext(), 8),
                           strlen(SPA_HANDLERID_VARIABLE) + 1),
      true, llvm::GlobalValue::PrivateLinkage, NULL, SPA_HANDLERID_VARIABLE);
  handlerIDVarName->setInitializer(llvm::ConstantDataArray::getString(
      module->getContext(), SPA_HANDLERID_VARIABLE, true));

  // Set up basic blocks to add entry handlers.
  llvm::BranchInst::Create(returnBB, nextHandlerBB);
  newEntryLevel();

  // return 0;
  llvm::ReturnInst::Create(
      module->getContext(),
      llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0, true)),
      returnBB);

  // 		module->dump();
  // 		entryFunction->dump();
}

void SPA::addInitFunction(llvm::Function *fn) {
  llvm::CallInst *initCall =
      llvm::CallInst::Create(fn, "", initHandlerPlaceHolder);
  initCall->setCallingConv(llvm::CallingConv::C);
  initCall->setTailCall(false);
}

void SPA::addEntryFunction(llvm::Function *fn) {
  // Check function signature.
  assert(!fn->isVarArg() && "Unsupported function type for entry function.");
  assert(fn->getReturnType()->getTypeID() == llvm::Type::VoidTyID &&
         "Unsupported function type for entry function.");
  assert(fn->getFunctionType()->getNumParams() == 0 &&
         "Unsupported function type for entry function.");

  // case x:
  // Create basic block for this case.
  llvm::BasicBlock *swBB =
      llvm::BasicBlock::Create(module->getContext(), "", entryFunction, 0);
  entryHandlerSwitchInst->addCase(
      llvm::ConstantInt::get(module->getContext(),
                             llvm::APInt(32, entryHandlerID++, true)),
      swBB);
  // Add call to spa_path_fork before calling entry handler.
  llvm::Function *spaPathForkFunction =
      module->getFunction(SPA_PATH_FORK_FUNCTION);
  assert(spaPathForkFunction && "spa_path_fork not defined in module.");
  // spa_path_fork();
  llvm::CallInst *spaPathForkCallInst =
      llvm::CallInst::Create(spaPathForkFunction, "", swBB);
  spaPathForkCallInst->setCallingConv(llvm::CallingConv::C);
  spaPathForkCallInst->setTailCall(false);
  // handlerx();
  llvm::CallInst *handlerCallInst = llvm::CallInst::Create(fn, "", swBB);
  handlerCallInst->setCallingConv(llvm::CallingConv::C);
  handlerCallInst->setTailCall(false);
  // break;
  llvm::BranchInst::Create(nextHandlerBB, swBB);
}

// 	void SPA::addSeedEntryFunction( unsigned int seedID, llvm::Function *fn ) {
// 		// case x:
// 		// Create basic block for this case.
// 		llvm::BasicBlock *swBB = llvm::BasicBlock::Create( module->getContext(),
// "", entryFunction, 0 );
// 		entryHandlerSwitchInst->addCase( llvm::ConstantInt::get(
// module->getContext(), llvm::APInt( 32, entryHandlerID++, true ) ), swBB );
//
// 		// spa_internal_SeedID = seedID;
// 		llvm::GlobalVariable *seedIDVar = module->getNamedGlobal ( SEED_ID_VAR_NAME
// );
// 		assert( seedIDVar && "SeedID variable not declared in module." );
// 		new StoreInst( ConstantInt::get( module->getContext(), APInt( 32,
// (uint64_t) seedID, false ) ), seedIDVar );
//
// 		// handlerx();
// 		llvm::CallInst *handlerCallInst = llvm::CallInst::Create( fn, "", swBB );
// 		handlerCallInst->setCallingConv( llvm::CallingConv::C );
// 		handlerCallInst->setTailCall( false );
// 		// break;
// 		llvm::BranchInst::Create(nextHandlerBB, swBB);
// 	}

void SPA::newEntryLevel() {
  // Get klee_int function.
  llvm::Function *kleeIntFunction = module->getFunction(KLEE_INT_FUNCTION);
  if (!kleeIntFunction) {
    kleeIntFunction = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::IntegerType::get(module->getContext(), 32),
            llvm::ArrayRef<llvm::Type *>(llvm::PointerType::get(
                llvm::IntegerType::get(module->getContext(), 8), 0)),
            false),
        llvm::GlobalValue::ExternalLinkage, KLEE_INT_FUNCTION, module);
    kleeIntFunction->setCallingConv(llvm::CallingConv::C);
  }

  // Remove the temporary branch instruction.
  nextHandlerBB->begin()->eraseFromParent();
  // handlerID = klee_int();
  llvm::Constant *idxsa[] = {
    llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0, true)),
    llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0, true))
  };
  llvm::ArrayRef<llvm::Constant *> idxsar(idxsa, 2);
  llvm::CallInst *kleeIntCall = llvm::CallInst::Create(
      kleeIntFunction,
      llvm::ConstantExpr::getGetElementPtr(handlerIDVarName, idxsar, false), "",
      nextHandlerBB);
  kleeIntCall->setCallingConv(llvm::CallingConv::C);
  kleeIntCall->setTailCall(false);
  // switch ( handlerID )
  entryHandlerSwitchInst =
      llvm::SwitchInst::Create(kleeIntCall, returnBB, 1, nextHandlerBB);
  // Entry handlers will be later added starting with id = 1.
  entryHandlerID = 1;

  nextHandlerBB =
      llvm::BasicBlock::Create(module->getContext(), "", entryFunction, 0);
  llvm::BranchInst::Create(returnBB, nextHandlerBB);
}

void SPA::addInitialValues(
    std::map<llvm::Value *,
             std::vector<std::vector<std::pair<bool, uint8_t> > > > values) {
  // Get malloc function.
  llvm::Function *mallocFunction = module->getFunction(MALLOC_FUNCTION);
  if (!mallocFunction) {
    mallocFunction = llvm::Function::Create(
        llvm::FunctionType::get(
            llvm::PointerType::get(
                llvm::IntegerType::get(module->getContext(), 8), 0),
            llvm::ArrayRef<llvm::Type *>(
                llvm::IntegerType::get(module->getContext(), 64)),
            false),
        llvm::GlobalValue::ExternalLinkage, "malloc", module);
    mallocFunction->setCallingConv(llvm::CallingConv::C);
  }

  // case x:
  // Create basic block for this case.
  llvm::BasicBlock *swBB =
      llvm::BasicBlock::Create(module->getContext(), "", entryFunction, 0);
  initValueSwitchInst->addCase(
      llvm::ConstantInt::get(module->getContext(),
                             llvm::APInt(32, initValueID++, true)),
      swBB);

  // Iterate over variables to initialize.
  for (auto varit : values) {
    // Store concrete values.
    // %1 = malloc( (numValues + 1) * 2 );
    llvm::CallInst *varMallocCallInst = llvm::CallInst::Create(
        mallocFunction,
        llvm::ConstantInt::get(
            module->getContext(),
            llvm::APInt(64, (varit.second.size() + 1) * 2 * 8, true)),
        "", swBB);
    varMallocCallInst->setCallingConv(llvm::CallingConv::C);
    varMallocCallInst->setTailCall(true);
    llvm::CastInst *varMalloc = new llvm::BitCastInst(
        varMallocCallInst,
        llvm::PointerType::get(
            llvm::PointerType::get(
                llvm::IntegerType::get(module->getContext(), 8), 0),
            0),
        "", swBB);
    // var = %1;
    new llvm::StoreInst(
        varMalloc,
        llvm::GetElementPtrInst::Create(
            varit.first, llvm::ConstantInt::get(module->getContext(),
                                                llvm::APInt(32, 0, true)),
            "", swBB),
        false, swBB);
    // Iterate distinct initial values for this variable.
    unsigned int offset = 0, numSymbolic = 0;
    for (auto valit : varit.second) {
      // %2 = malloc( values[it].length );
      llvm::CallInst *valMallocCallInst = llvm::CallInst::Create(
          mallocFunction,
          llvm::ConstantInt::get(module->getContext(),
                                 llvm::APInt(64, valit.size(), true)),
          "", swBB);
      valMallocCallInst->setCallingConv(llvm::CallingConv::C);
      valMallocCallInst->setTailCall(true);
      llvm::CastInst *valMalloc = new llvm::BitCastInst(
          valMallocCallInst,
          llvm::PointerType::get(
              llvm::IntegerType::get(module->getContext(), 8), 0),
          "", swBB);
      // %1[offset+0] = %2;
      new llvm::StoreInst(valMalloc,
                          llvm::GetElementPtrInst::Create(
                              varMalloc, llvm::ConstantInt::get(
                                             module->getContext(),
                                             llvm::APInt(32, offset + 0, true)),
                              "", swBB),
                          false, swBB);
      // %3 = malloc( valueMasks[it].length );
      llvm::CallInst *maskMallocCallInst = llvm::CallInst::Create(
          mallocFunction,
          llvm::ConstantInt::get(module->getContext(),
                                 llvm::APInt(64, valit.size(), true)),
          "", swBB);
      maskMallocCallInst->setCallingConv(llvm::CallingConv::C);
      maskMallocCallInst->setTailCall(true);
      llvm::CastInst *maskMalloc = new llvm::BitCastInst(
          maskMallocCallInst,
          llvm::PointerType::get(
              llvm::IntegerType::get(module->getContext(), 8), 0),
          "", swBB);
      // %1[offset+1] = %2;
      new llvm::StoreInst(maskMalloc,
                          llvm::GetElementPtrInst::Create(
                              varMalloc, llvm::ConstantInt::get(
                                             module->getContext(),
                                             llvm::APInt(32, offset + 1, true)),
                              "", swBB),
                          false, swBB);
      // Iterator over initial value bytes and mask.
      bool containsSymbol = false;
      for (unsigned int i = 0; i < valit.size(); i++) {
        // %2[i] = value[it][i];
        new llvm::StoreInst(
            llvm::ConstantInt::get(module->getContext(),
                                   llvm::APInt(8, valit[i].second, true)),
            llvm::GetElementPtrInst::Create(
                valMalloc, llvm::ConstantInt::get(module->getContext(),
                                                  llvm::APInt(32, i, true)),
                "", swBB),
            false, swBB);
        // %3[i] = valueMasks[it][i];
        new llvm::StoreInst(
            llvm::ConstantInt::get(
                module->getContext(),
                llvm::APInt(8, valit[i].first ? 1 : 0, true)),
            llvm::GetElementPtrInst::Create(
                maskMalloc, llvm::ConstantInt::get(module->getContext(),
                                                   llvm::APInt(32, i, true)),
                "", swBB),
            false, swBB);
        if (!valit[i].first)
          containsSymbol = true;
      }
      if (containsSymbol) {
        assert(++numSymbolic <= 1 && "More than one symbolic initial values "
                                     "declared for a single variable.");
      }
      offset += 2;
    }
  }

  // break;
  llvm::BranchInst::Create(firstHandlerBB, swBB);
}

void SPA::setSenderPathLoader(PathLoader *pathLoader, bool follow) {
  senderPaths = pathLoader;
  followSenderPaths = follow;
}

void SPA::addValueMapping(std::string senderVar, std::string receiverVar) {
  klee::klee_message("   Adding value mapping: %s = %s", receiverVar.c_str(),
                     senderVar.c_str());
  seedSymbolMappings[receiverVar] = senderVar;
}

void SPA::mapInputsToOutputs() {
  klee::klee_message("   Loading input/output value mappings.");

  llvm::Function *fn = module->getFunction(SPA_INPUT_ANNOTATION_FUNCTION);
  assert(fn && "spa_input not found in module.");

  CG cg(module);
  std::set<llvm::Instruction *> annotations = cg.getDefiniteCallers(fn);

  for (auto it : annotations) {
    const llvm::CallInst *callInst;
    assert(callInst = dyn_cast<llvm::CallInst>(it));
    assert(callInst->getNumArgOperands() == 5);
    llvm::GlobalVariable *gv;
    assert(gv = dyn_cast<llvm::GlobalVariable>(callInst->getArgOperand(2)
                                                   ->stripPointerCasts()));
    if (llvm::ConstantDataArray *cda =
            dyn_cast<llvm::ConstantDataArray>(gv->getInitializer())) {
      std::string receiverVarName = cda->getAsCString().str();

      // Check if input message.
      if (receiverVarName.compare(0, strlen(SPA_MESSAGE_INPUT_PREFIX),
                                  SPA_MESSAGE_INPUT_PREFIX) == 0) {
        std::string senderVarName =
            std::string(SPA_MESSAGE_OUTPUT_PREFIX) +
            receiverVarName.substr(strlen(SPA_MESSAGE_INPUT_PREFIX));
        addValueMapping(senderVarName, receiverVarName);
      }
    }
  }
}

void SPA::mapCommonInputs() {
  klee::klee_message("   Loading input/input value mappings.");

  llvm::Function *fn = module->getFunction(SPA_INPUT_ANNOTATION_FUNCTION);
  assert(fn && "spa_input not found in module.");

  CG cg(module);
  std::set<llvm::Instruction *> annotations = cg.getDefiniteCallers(fn);

  for (auto it : annotations) {
    const llvm::CallInst *callInst;
    assert(callInst = dyn_cast<llvm::CallInst>(it));
    assert(callInst->getNumArgOperands() == 5);
    llvm::GlobalVariable *gv;
    assert(gv = dyn_cast<llvm::GlobalVariable>(callInst->getArgOperand(2)
                                                   ->stripPointerCasts()));
    llvm::ConstantDataArray *cda;
    assert(cda = dyn_cast<llvm::ConstantDataArray>(gv->getInitializer()));
    std::string receiverVarName = cda->getAsCString().str();

    // Check if input message.
    if (receiverVarName.compare(0, strlen(SPA_MESSAGE_INPUT_PREFIX),
                                SPA_MESSAGE_INPUT_PREFIX) == 0) {
      std::string senderVarName =
          std::string(SPA_MESSAGE_INPUT_PREFIX) +
          receiverVarName.substr(strlen(SPA_MESSAGE_INPUT_PREFIX));
      addValueMapping(senderVarName, receiverVarName);
    }
  }
}

void SPA::mapSockets() {
  klee::klee_message("   Mapping socket symbols.");
  connectSockets = true;
}

void SPA::start() {
  // Set the participant name for symbol naming.
  llvm::GlobalVariable *participantNameVar =
      module->getNamedGlobal(SPA_PARTICIPANTNAME_VARIABLE);
  assert(participantNameVar &&
         "participantName variable not declared in module.");
  llvm::Constant *participantNameConstant =
      llvm::ConstantDataArray::getString(module->getContext(), ParticipantName);
  std::vector<llvm::Constant *> indices;
  indices.push_back(
      llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0)));
  indices.push_back(
      llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0)));
  participantNameVar->setInitializer(llvm::ConstantExpr::getGetElementPtr(
      new llvm::GlobalVariable(*module, participantNameConstant->getType(),
                               true, llvm::GlobalValue::PrivateLinkage,
                               participantNameConstant,
                               SPA_PARTICIPANTNAME_VARIABLE "_value"),
      indices));

  // Set the source address for default binding.
  if (IP.empty()) { // Default to loop-back network.
    IP = "127.0.0.1";
  }
  llvm::GlobalVariable *defaultBindAddrVar =
      module->getNamedGlobal(SPA_DEFAULTBINDADDR_VARIABLE);
  assert(defaultBindAddrVar &&
         "defaultBindAddrVar variable not declared in module.");
  llvm::Constant *defaultBindAddrConstant =
      llvm::ConstantDataArray::getString(module->getContext(), IP);
  indices.clear();
  indices.push_back(
      llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0)));
  indices.push_back(
      llvm::ConstantInt::get(module->getContext(), llvm::APInt(32, 0)));
  defaultBindAddrVar->setInitializer(llvm::ConstantExpr::getGetElementPtr(
      new llvm::GlobalVariable(*module, defaultBindAddrConstant->getType(),
                               true, llvm::GlobalValue::PrivateLinkage,
                               defaultBindAddrConstant,
                               SPA_DEFAULTBINDADDR_VARIABLE "_value"),
      indices));

  bool outputFP = false;
  for (auto it : outputFilteredPaths) {
    if (it) {
      outputFP = true;
      break;
    }
  }
  assert((outputFP || outputTerminalPaths || outputDone || outputLogExhausted ||
          checkpointFilter.getSubFilters().size() > 1 ||
          !checkpointWhitelist.getWhitelist().empty()) &&
         "No points to output data from.");

  klee::Interpreter::InterpreterOptions IOpts;
  IOpts.MakeConcreteSymbolic = false;
  klee::Interpreter::ModuleOptions Opts(
      /*LibraryDir=*/ KLEE_DIR "/" RUNTIME_CONFIGURATION "/lib",
      /*Optimize=*/ false, /*CheckDivZero=*/ true, /*CheckOvershift=*/ true);

  if (!stateUtilities.empty()) {
    klee::klee_message("Replacing searcher with SPA utility framework.");
    SpaSearcher *spaSearcher = new SpaSearcher(stateUtilities);
    spaSearcher->addFilteringEventHandler(this);
    IOpts.searcher = spaSearcher;
  }

  executor =
      static_cast<klee::Executor *>(klee::Interpreter::create(IOpts, this));
  executor->addInterpreterEventListener(this);

  externalsAndGlobalsCheck(
      module = const_cast<llvm::Module *>(executor->setModule(module, Opts)));

  int pArgc = 1;
  char argv0[] = "";
  char *pArgv[2] = { argv0, NULL };
  char *pEnvp[1] = { NULL };

  executor->runFunctionAsMain(entryFunction, pArgc, pArgv, pEnvp);

  delete executor;

  uint64_t queries = *klee::theStatisticManager->getStatisticByName("Queries");
  uint64_t queriesValid =
      *klee::theStatisticManager->getStatisticByName("QueriesValid");
  uint64_t queriesInvalid =
      *klee::theStatisticManager->getStatisticByName("QueriesInvalid");
  uint64_t queryCounterexamples =
      *klee::theStatisticManager->getStatisticByName("QueriesCEX");
  uint64_t queryConstructs =
      *klee::theStatisticManager->getStatisticByName("QueriesConstructs");
  uint64_t instructions =
      *klee::theStatisticManager->getStatisticByName("Instructions");
  uint64_t forks = *klee::theStatisticManager->getStatisticByName("Forks");

  klee::klee_message("KLEE: done: explored paths = %ld\n", 1 + forks);
  if (queries)
    klee::klee_message("KLEE: done: avg. constructs per query = %ld\n",
                       queryConstructs / queries);
  klee::klee_message("KLEE: done: total queries = %ld\n", queries);
  klee::klee_message("KLEE: done: valid queries = %ld\n", queriesValid);
  klee::klee_message("KLEE: done: invalid queries = %ld\n", queriesInvalid);
  klee::klee_message("KLEE: done: query cex = %ld\n", queryCounterexamples);
  klee::klee_message("KLEE: done: total instructions = %ld\n", instructions);
}

void SPA::showStats() {
  klee::klee_message(
      "Checkpoints: %ld; TerminalPaths: %ld; Outputted: %ld; Filtered: %ld",
      checkpointsFound, terminalPathsFound, outputtedPaths, filteredPathsFound);
}

void SPA::processPath(klee::ExecutionState *state) {
  if (state->isReplayingSpaLog()) {
    return;
  }

  Path path(state, executor->getSolver());

  if (!pathFilter || pathFilter->checkPath(path)) {
    klee::klee_message("Outputting path.");
    output << path;
    outputtedPaths++;

    if (MaxPaths && outputtedPaths >= (unsigned) MaxPaths) {
      klee::klee_message("Found specified number of paths. Halting.");
      executor->setHaltExecution(true);
    }
  }
}

void SPA::onStateBranched(klee::ExecutionState *kState,
                          klee::ExecutionState *parent, int index) {
  kState->branchDecisions
      .push_back(std::make_pair(kState->prevPC->inst, index));
}

void SPA::onStep(klee::ExecutionState *kState) {
  if (StepDebug) {
    klee::klee_message("Current state at:");
    kState->dumpStack(llvm::errs());
  }

  if (!DumpCov.empty()) {
    coverage.insert(kState->pc->inst);

    static auto last = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last)
            .count() > DUMP_COVERAGE_PERIOD) {
      last = now;

      klee::klee_message("Dumping global coverage data to: %s",
                         DumpCov.c_str());
      std::ofstream dumpFile(DumpCov.c_str());
      assert(dumpFile.is_open() && "Unable to open path dump file.");
      std::map<std::string, std::set<long> > lineCoverage;
      for (auto inst : coverage) {
        if (llvm::MDNode *node = inst->getMetadata("dbg")) {
          llvm::DILocation loc(node);
          lineCoverage[loc.getDirectory().str() + "/" + loc.getFilename().str()]
              .insert(loc.getLineNumber());
        }
      }
      for (auto file : lineCoverage) {
        dumpFile << file.first;
        for (auto line : file.second) {
          dumpFile << " " << line;
        }
        dumpFile << '\n';
      }
      dumpFile.close();
    }
  }

  kState->instructionCoverage.insert(kState->pc->inst);

  if (checkpointFilter.checkStep(kState->prevPC->inst, kState->pc->inst) ||
      checkpointFilter.checkStep(NULL, kState->pc->inst) ||
      checkpointFilter.checkStep(kState->prevPC->inst, NULL)) {
    klee::klee_message("Processing checkpoint path at:");
    kState->dumpStack(llvm::errs());
    checkpointsFound++;
    processPath(kState);
    showStats();
  }

  if ((stopPointFilter.checkStep(kState->prevPC->inst, kState->pc->inst) ||
       stopPointFilter.checkStep(NULL, kState->pc->inst) ||
       stopPointFilter.checkStep(kState->prevPC->inst, NULL)) &&
      !kState->isReplayingSpaLog()) {
    klee::klee_message("Path reached stop point:");
    kState->dumpStack(llvm::errs());
    executor->terminateStateEarly(*kState, "Reached stop point.");
  }
}

void SPA::onStateTerminateEarly(klee::ExecutionState *kState) {
  klee::klee_message("State terminated early at:");
  kState->dumpStack(llvm::errs());

  if (kState->filtered) {
    klee::klee_message("Filtered path destroyed.");
  } else {
    bool logExhausted = !kState->isReplayingSpaLog();
    if (outputLogExhausted && logExhausted) {
      klee::klee_message("Processing path with exhausted symbol log.");
    }

    terminalPathsFound++;
    if (outputTerminalPaths || (outputLogExhausted && logExhausted)) {
      assert(kState);
      klee::klee_message("Processing terminal path.");
      processPath(kState);
    }
  }
  showStats();
}

void SPA::onStateTerminateError(klee::ExecutionState *kState) {
  klee::klee_message("State terminated with error at:");
  kState->dumpStack(llvm::errs());

  bool logExhausted = !kState->isReplayingSpaLog();
  if (outputLogExhausted && logExhausted) {
    klee::klee_message("Processing path with exhausted symbol log.");
  }

  terminalPathsFound++;
  if (outputTerminalPaths || (outputLogExhausted && logExhausted)) {
    assert(kState);
    klee::klee_message("Processing terminal path.");
    processPath(kState);
  }
  showStats();
}

void SPA::onStateTerminateDone(klee::ExecutionState *kState) {
  klee::klee_message("State terminated naturally at:");
  kState->dumpStack(llvm::errs());

  bool logExhausted = !kState->isReplayingSpaLog();
  if (outputLogExhausted && logExhausted) {
    klee::klee_message("Processing path with exhausted symbol log.");
  }

  terminalPathsFound++;
  if (outputTerminalPaths || outputDone ||
      (outputLogExhausted && logExhausted)) {
    assert(kState);
    klee::klee_message("Processing terminal path.");
    processPath(kState);
  }
  showStats();
}

void SPA::onStateFiltered(klee::ExecutionState *state, unsigned int id) {
  filteredPathsFound++;

  if (outputFilteredPaths[id]) {
    assert(state);

    klee::klee_message("Processing filtered path.");

    processPath(state);
  }
  showStats();
}
}

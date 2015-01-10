/*
 * SPA - Systematic Protocol Analysis Framework
 */

#ifndef __DbgLineIF_H__
#define __DbgLineIF_H__

#include <spa/InstructionFilter.h>
#include <spa/CFG.h>

namespace SPA {
class DbgLineIF : public InstructionFilter {
private:
  std::set<llvm::Instruction *> dbgZone;
  bool entering;

  DbgLineIF(std::set<llvm::Instruction *> dbgZone, bool entering)
      : dbgZone(dbgZone), entering(entering) {}

public:
  static DbgLineIF *parse(llvm::Module *module, std::string dbgPoint);

  bool checkInstruction(llvm::Instruction *instruction) {
    return dbgZone.count(instruction);
  }

  bool checkStep(llvm::Instruction *preInstruction,
                 llvm::Instruction *postInstruction) {
    if (preInstruction && postInstruction) {
      if (entering) {
        return (!dbgZone.count(preInstruction)) &&
               dbgZone.count(postInstruction);
      } else {
        return dbgZone.count(preInstruction) && !dbgZone.count(postInstruction);
      }
    } else {
      return false;
    }
  }
};
}

#endif // #ifndef __DbgLineIF_H__

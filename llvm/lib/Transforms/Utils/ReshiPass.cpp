#include "llvm/Transforms/Utils/ReshiPass.h"
#include "llvm/IR/Function.h"

using namespace llvm;

PreservedAnalyses ReshiPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {
  errs() << F.getName() << "\n";
  return PreservedAnalyses::all();
}

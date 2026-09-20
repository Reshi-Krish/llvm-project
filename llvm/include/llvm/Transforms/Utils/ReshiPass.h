#ifndef LLVM_TRANSFORMS_NEW_RESHI_H
#define LLVM_TRANSFORMS_NEW_RESHI_H

#include "llvm/IR/PassManager.h"

namespace llvm {

class ReshiPass : public OptionalPassInfoMixin<ReshiPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_NEW_RESHI_H
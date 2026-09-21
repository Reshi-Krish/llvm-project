#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

int main(int argc, char **argv) {
  LLVMContext context;
  SMDiagnostic error;
if(argc < 2) {
return -1;
}

  auto module = parseIRFile(argv[1], error, context);
if(module ==NULL ){
 return -1;
}
  auto func = module->getFunction("div");
if(func ==NULL ){
 return -1;
}
  errs() << "function name  " << func->getName() << "\n";
  for (auto b = func->begin(), e = func->end(); b != e; b++) {
    errs() << "BasicBlock: " << b->getName() << "\n";
    errs() << "Size:" << b->size() << "\n";
    for (auto i = b->begin(); i != b->end(); ++i) {
      errs() << "  " << *i << "\n";
    }
  }
  return 0;
}


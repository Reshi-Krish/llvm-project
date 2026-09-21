#include "llvm/Transforms/Utils/ReshiPass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"

using namespace llvm;

namespace {

bool nanAlgebricXopIdInternal(BinaryOperator *BI)
{
      auto op1 = BI->getOperand(0);//first operand
      auto op2 = BI->getOperand(1);//second operand

      Value *finalci = NULL;//initialize to NULL

      if(op1 == op2) {//both operands are the same value
        switch(BI->getOpcode()) {//check op code of the operator
        default:
          break;
        case Instruction::Add: {
          //          errs() << "applying x + x = x << 1 for : " << BI << "\n";
          auto shift = ConstantInt::get(op1->getType(), 1);//shift left by 1
          finalci = BinaryOperator::CreateShl(op1, shift, "", BI->getIterator());
          break;
        }
        case Instruction::Sub:
          //          errs() << "applying x - x = 0 for : " << BI << "\n";
          finalci = ConstantInt::get(op1->getType(), 0);
          break;
        case Instruction::SDiv:
        case Instruction::UDiv:
          //          errs() << "applying x / x = 1 for : " << BI << "\n";
          finalci = ConstantInt::get(op1->getType(), 1);
          break;
        }

        if(finalci) {//if not NULL
          errs()<<"Replacing all uses of " << *BI<<" with its identity value\n";
          auto V = dyn_cast<Instruction>(BI);
          assert(V && V->hasNUsesOrMore(1));
          V->replaceAllUsesWith(finalci);//replace all uses of BI with the computed identity value
          return true;
        }

        return false;
      }

      auto op2c = dyn_cast<Constant>(op2);//typecase second operand to constant
      if(!op2c)
        return false;

      switch(BI->getOpcode()) {//check op code of the operator
      default:
        break;
      case Instruction::Add:
      case Instruction::Sub:
        if(op2c->isNullValue()) {//if value of constant is NULL/Zero
//          errs() << "applying x +- ID = x for : " << BI << "\n";
          finalci = op1;//operand one
        }

        break;
      case Instruction::Mul: {
        auto op2ci = dyn_cast<ConstantInt>(op2c);//power-of-2 check lives on ConstantInt's APInt, not Constant
        if(op2ci && op2ci->getValue().isOne()) {//if value of constant is One
//          errs() << "applying x * ID = x for : " << BI << "\n";
          finalci = op1;//operand one
        } else if(op2ci && op2ci->getValue().isPowerOf2()) {//if value of constant is power of 2
//          errs() << "applying x * ID = x for : " << BI << "\n";
          auto shift = ConstantInt::get(op2ci->getType(), op2ci->getValue().countTrailingZeros());//get the count of trailing zeros
          finalci = BinaryOperator::CreateShl(op1, shift, "", BI->getIterator());//operand 1 shifted left by the count of trailing zeros
        }
        break;
      }
      case Instruction::SDiv:
      case Instruction::UDiv:
        if(op2c->isOneValue()) {//if value of constant is One
  //        errs() << "applying x */ ID = x for : " << BI << "\n";
          finalci = op1;//operand one
        }

        break;
      }

      if(finalci) {//if not NULL
        errs()<<"Replacing all uses of " << *BI<<" with its first operand\n";
        auto V = dyn_cast<Instruction>(BI);
        assert(V && V->hasNUsesOrMore(1));
//      errs()<<"Replacing all uses with"<< V<<"\n";
        V->replaceAllUsesWith(finalci);//replace all used of BI with its operand one
        return true;
      }

      return false;
}

bool nanAlgebraicXopId(BasicBlock &BB)
{
  bool changed = false;

  for(Instruction &I : BB) {//iterate over all instructions in the block
    auto BI = dyn_cast<BinaryOperator>(&I);
    if(!BI)
      continue;

    changed |= nanAlgebricXopIdInternal(BI);
  }

  return changed;
}

bool nanPerformLocalOpt(BasicBlock &BB)
{
  bool changed = false;

  errs() << "optimizing BB " << BB.getName() << " of function " << BB.getParent()->getName() << "\n";

 changed ^= nanAlgebraicXopId(BB);//due to side effect produced by PROGRAMMER with no change in program semantics.  
//  changed ^= nanReduceStrength(BB); // "Reduce strength X* 4 --> shl x, 2 and  X/4 --> shr x, 2
  return changed;
}

void dumpBasicBlock(BasicBlock &BB, const char *title)
{
  errs() << "=== " << title << " ===" << "\n";

  for(Instruction &I : BB) {
    errs() << I << "\n";
  }

  errs() << "=================" << "\n\n";
}

} // namespace

PreservedAnalyses ReshiPass::run(Function &F,
                                      FunctionAnalysisManager &AM) {
  bool changed = false;

  PromotePass().run(F, AM);//promote allocas to SSA registers (mem2reg) so identity checks can see repeated uses of the same value

  for(BasicBlock &BB : F) {//iterate over all basic blocks in the function
          //XopID
          //strength reduction x*4  shl x , 2
//    dumpBasicBlock(BB, "BEFORE OPT");
    nanPerformLocalOpt(BB);//Optimization local to basic block: XopID and strength reduction.
    dumpBasicBlock(BB, "AFTER  OPT");
  }

  return PreservedAnalyses::all();
}

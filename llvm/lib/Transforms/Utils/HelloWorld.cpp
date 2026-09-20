#include "llvm/Transforms/Utils/HelloWorld.h"//included header file in llvm/inlcude folders
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;

bool nanAlgebricXopIdInternal(BinaryOperator *BI)
{
      auto op1 = BI->getOperand(0);//first operand
      auto op2 = BI->getOperand(1);//second operand

      auto op2c = dyn_cast<Constant>(op2);//typecase second operand to constant
      if(!op2c)
        return false;

      Value *finalci = NULL;//initialize to NULL

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
      case Instruction::Mul:
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
errs()<<"inside binary operation with identity element\n";
  bool changed = false;
for (auto I = BB.begin(), End = BB.end()/*initial*/; I != End/*termination*/; ) {
        changed=false;
  llvm::Instruction *Inst = &*I;//llvm::Instruction from Iteraror
  I++;//increment the iterator I
    if(BinaryOperator *BI = dyn_cast<BinaryOperator>(Inst)) {//typecast instruction to Binary operation
  //  errs()<<"Inside Binary Operator: "<< *Inst<<"\n";//BI not NULL
      changed = nanAlgebricXopIdInternal(BI);//if changed is true (XopID, uses of BI replaced with its operand one. 

      if(!changed && BI->isCommutative()) {//if changed is false and operator is commutative, need to check if we have IDopX
   // swap operands if operator in commutative:+, * etc.
        BI->swapOperands();//LLVM swap operands 
        changed = nanAlgebricXopIdInternal(BI);//check for Identity element as second operand after swap of operand
 BI->swapOperands();//LLVM API Function, do swap again
      }
  if(changed){
        //    errs()<<"before delete of : "<< *Inst<<"\n";

          Inst->eraseFromParent(); //If identity element is found, delete the instruction.
          //  errs()<<"after delete\n";
  }

    }
}
  return changed;
}

bool nanReduceStrength(BasicBlock &BB)
{
errs()<<"Inside Strength Reduction\n";

bool changed = false;
//for(init; termination-condition; increment)
for (auto I = BB.begin(), End = BB.end()/*init*/; I != End/*termination-condition*/; ) {//iterate over instructions in the basic block
        changed=false;
  llvm::Instruction *Inst = &*I;//get Instruction from iterator. syntax you have to follow
  I++;//increment the iterator I
    if(BinaryOperator *BI = dyn_cast<BinaryOperator>(Inst)) {//if instruction is a binary operator
//errs()<<"inside binary operator strength reduction\n";
      auto op1 = BI->getOperand(0);//operand one
      auto op2 = BI->getOperand(1);//operand two

      auto op2ci = dyn_cast<llvm::ConstantInt>(op2);//type cast operand two to a constant int
//if(op2ci)errs()<<"inside binary operator: "<<*Inst<<"\n";
      if(!op2ci && BI->isCommutative()){
        BI->swapOperands();//LLVM API Function
       op2 = BI->getOperand(1);//operand two
       op1 = BI->getOperand(0);//operand one
      op2ci = dyn_cast<llvm::ConstantInt>(op2);//type cast operand two to a constant int
      }
      if(!op2ci)continue;
//errs()<<"inside binary operatpr power of two ops SR\n";
      auto op2api = op2ci->getValue();//getting value of constant int
      if(!op2api.isPowerOf2())//check whether the value is power of two
        continue;

  //    errs() << "x op POWER_OF_TWO detected: " << *Inst << "\n";

      int logb2 = op2api.exactLogBase2();//log2 val

      assert(logb2 > 0);//makesure logb2 is >0, safety check

      Value *finalci = NULL;//llvm::Value class

      switch(BI->getOpcode()) {
      default:
        break;
      case Instruction::Mul:
  //      errs() << "reducing multiplication to left shift in: " << BI << "\n";
        /*create a new binary operator or new instruction shr*/
        finalci = BinaryOperator::Create(Instruction::Shl/*operator*/, op1/*operand1*/, ConstantInt::get(op1->getType(), logb2)/*operand2*/);
        changed=true;
        break;
      case Instruction::SDiv:
      case Instruction::UDiv:
//        errs() << "reducing division to right shift in: " << BI << "\n";
        /*create a new binary operator or new instruction shl*/
        finalci = BinaryOperator::Create(Instruction::AShr, op1, ConstantInt::get(op1->getType(), logb2));
        changed=true;
        break;
      }
if(finalci) {//if new instruction is created, delete old instruction, and insert new instruction to basic block
              Instruction *newinst=dyn_cast<Instruction>(finalci);//typecase finalci to llvm::Instruction class
            newinst->insertBefore(I);//I points to next instruction after mul, new instruction inserted before I
       Inst->replaceAllUsesWith(finalci);//replace all used of  mul/div  with new instuction
                                errs()<<"Replacing "<< *Inst <<" with " <<*newinst<<"\n";
       Inst->eraseFromParent();//remove the instruction mul/div from basis block

        changed = true;
      }
    }
  }

  return changed;
}
bool nanPerformLocalOpt(BasicBlock &BB)
{
  bool changed = false;

  errs() << "optimizing BB " << BB.getName() << " of function " << BB.getParent()->getName() << "\n";

 changed ^= nanAlgebraicXopId(BB);//due to side effect produced by PROGRAMMER with no change in program semantics.  
 changed ^= nanReduceStrength(BB); // "Reduce strength X* 4 --> shl x, 2 and  X/4 --> shr x, 2
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

PreservedAnalyses HelloWorldPass::run(Function &F, FunctionAnalysisManager &AM) {

  bool changed = false;

  for(BasicBlock &BB : F) {//iterate over all basic blocks in the function
          //XopID
          //strength reduction x*4  shl x , 2
//    dumpBasicBlock(BB, "BEFORE OPT");
    nanPerformLocalOpt(BB);//Optimization local to basic block: XopID and strength reduction.
    dumpBasicBlock(BB, "AFTER  OPT");
  }

  return PreservedAnalyses::all();
}

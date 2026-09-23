#include "llvm/Transforms/Utils/ReshiPass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/CFG.h"

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
          V->eraseFromParent();//BI is now dead, remove it from the block
          return true;
        }

        return false;
      }

      auto op2c = dyn_cast<Constant>(op2);//typecase second operand to constant
      auto op1c = dyn_cast<Constant>(op1);//typecast first operand to constant, for the commutative (Add/Mul) case

      if(!op2c && !op1c)
        return false;

      if(op2c) {
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
      }

      if(!finalci && op1c) {
        //op1 is (also) a constant, e.g. op2c matched Constant but wasn't a usable ConstantInt (undef); Sub/Div aren't commutative so only Add/Mul have a free identity here
        switch(BI->getOpcode()) {
        default:
          break;
        case Instruction::Add:
          if(op1c->isNullValue()) {//ID + x = x
//            errs() << "applying ID + x = x for : " << BI << "\n";
            finalci = op2;//operand two
          }
          break;
        case Instruction::Sub:
          if(op1c->isNullValue()) {//0 - x = -x
//            errs() << "applying 0 - x = -x for : " << BI << "\n";
            finalci = BinaryOperator::CreateNeg(op2, "", BI->getIterator());//negate operand two
          }
          break;
        case Instruction::Mul: {
          auto op1ci = dyn_cast<ConstantInt>(op1c);
          if(op1ci && op1ci->getValue().isOne()) {//ID * x = x
//            errs() << "applying ID * x = x for : " << BI << "\n";
            finalci = op2;//operand two
          } else if(op1ci && op1ci->getValue().isPowerOf2()) {//C * x = x << log2(C)
//            errs() << "applying C * x = x << shift for : " << BI << "\n";
            auto shift = ConstantInt::get(op1ci->getType(), op1ci->getValue().countTrailingZeros());
            finalci = BinaryOperator::CreateShl(op2, shift, "", BI->getIterator());//operand 2 shifted left by the count of trailing zeros
          }
          break;
        }
        }
      }

      if(finalci) {//if not NULL
        errs()<<"Replacing all uses of " << *BI<<" with its first operand\n";
        auto V = dyn_cast<Instruction>(BI);
        assert(V && V->hasNUsesOrMore(1));
//      errs()<<"Replacing all uses with"<< V<<"\n";
        V->replaceAllUsesWith(finalci);//replace all used of BI with its operand one
        V->eraseFromParent();//BI is now dead, remove it from the block
        return true;
      }

      return false;
}

bool nanConstantFoldInternal(BinaryOperator *BI)
{
      auto op1c = dyn_cast<ConstantInt>(BI->getOperand(0));//typecast first operand to constant int
      auto op2c = dyn_cast<ConstantInt>(BI->getOperand(1));//typecast second operand to constant int

      if(!op1c || !op2c)//both operands must be constant ints to fold
        return false;

      auto &lhs = op1c->getValue();//APInt value of operand one
      auto &rhs = op2c->getValue();//APInt value of operand two

      APInt result;//holds the folded result

      switch(BI->getOpcode()) {//check op code of the operator
      default:
        return false;//opcode not handled for constant folding
      case Instruction::Add:
        result = lhs + rhs;
        break;
      case Instruction::Sub:
        result = lhs - rhs;
        break;
      case Instruction::Mul:
        result = lhs * rhs;
        break;
      case Instruction::SDiv:
        if(rhs.isZero())//division by zero is undefined, do not fold
          return false;
        result = lhs.sdiv(rhs);
        break;
      case Instruction::UDiv:
        if(rhs.isZero())//division by zero is undefined, do not fold
          return false;
        result = lhs.udiv(rhs);
        break;
      case Instruction::And:
        result = lhs & rhs;
        break;
      case Instruction::Or:
        result = lhs | rhs;
        break;
      case Instruction::Xor:
        result = lhs ^ rhs;
        break;
      }

      auto finalci = ConstantInt::get(BI->getType(), result);//build the constant for the folded result

      errs()<<"Constant folding " << *BI << " into " << *finalci << "\n";
      auto V = dyn_cast<Instruction>(BI);
      assert(V && V->hasNUsesOrMore(1));
      V->replaceAllUsesWith(finalci);//replace all uses of BI with the folded constant
      V->eraseFromParent();//BI is now dead, remove it from the block

      return true;
}

bool nanConstantFold(BasicBlock &BB)
{
  bool changed = false;

  for(Instruction &I : make_early_inc_range(BB)) {//iterate over all instructions in the block; BI may be erased, so advance the iterator first
    auto BI = dyn_cast<BinaryOperator>(&I);
    if(!BI)
      continue;

    changed |= nanConstantFoldInternal(BI);
  }

  return changed;
}

bool nanAlgebraicXopId(BasicBlock &BB)
{
  bool changed = false;

  for(Instruction &I : make_early_inc_range(BB)) {//iterate over all instructions in the block; BI may be erased, so advance the iterator first
    auto BI = dyn_cast<BinaryOperator>(&I);
    if(!BI)
      continue;

    changed |= nanAlgebricXopIdInternal(BI);
  }

  return changed;
}

bool nanPropagateAllocaConstants(BasicBlock &BB)
{
  bool changed = false;
  DenseMap<Value *, Constant *> ConstVals;//pointer -> last known constant stored into it, valid until a store of a non-constant or a call may clobber it

  for(Instruction &I : make_early_inc_range(BB)) {//iterate over all instructions in the block; I may be erased, so advance the iterator first
    if(isa<CallInst>(&I)) {
      ConstVals.clear();//a call may write through a pointer we can't see, forget everything we knew
      continue;
    }

    if(auto *SI = dyn_cast<StoreInst>(&I)) {
      auto *Ptr = SI->getPointerOperand();

      if(SI->isVolatile()) {
        ConstVals.erase(Ptr);
        continue;
      }

      if(auto *C = dyn_cast<Constant>(SI->getValueOperand())) {
        errs() << "Tracking constant store " << *SI << "\n";
        ConstVals[Ptr] = C;//record the newly stored constant
      } else {
        ConstVals.erase(Ptr);//value stored is not a compile-time constant, forget what we knew
      }
      continue;
    }

    if(auto *LI = dyn_cast<LoadInst>(&I)) {
      if(LI->isVolatile())
        continue;

      auto *Ptr = LI->getPointerOperand();
      auto It = ConstVals.find(Ptr);
      if(It == ConstVals.end() || It->second->getType() != LI->getType())
        continue;

      errs() << "Replacing load " << *LI << " with constant " << *It->second << "\n";
      LI->replaceAllUsesWith(It->second);//replace the load with the known constant
      LI->eraseFromParent();//load is now dead, remove it from the block
      changed = true;
    }
  }

  return changed;
}

bool nanEliminateCommonSubexpressions(BasicBlock &BB)
{
  bool changed = false;
  //hash of (opcode, type, operands) -> earlier instructions with that hash, to be checked with isIdenticalTo
  DenseMap<hash_code, SmallVector<Instruction *, 2>> ExprTable;
  DenseMap<Value *, LoadInst *> LoadTable;//pointer -> last non-volatile load from it, valid until a store/call may clobber memory

  for(Instruction &I : make_early_inc_range(BB)) {//iterate over all instructions in the block; I may be erased, so advance the iterator first
    if(isa<CallInst>(&I) || isa<StoreInst>(&I)) {
      LoadTable.clear();//no alias analysis here: conservatively forget every tracked load, it may have been clobbered
      continue;
    }

    if(auto *LI = dyn_cast<LoadInst>(&I)) {
      if(LI->isVolatile())
        continue;

      auto *Ptr = LI->getPointerOperand();
      auto It = LoadTable.find(Ptr);
      if(It != LoadTable.end() && It->second->getType() == LI->getType()) {//same pointer, same loaded type, nothing clobbered it since
        errs() << "CSE: replacing redundant load " << *LI << " with earlier load " << *It->second << "\n";
        LI->replaceAllUsesWith(It->second);//reuse the earlier loaded value
        LI->eraseFromParent();//this reload is now dead, remove it from the block
        changed = true;
        continue;
      }

      LoadTable[Ptr] = LI;//first (or freshest) load from this pointer, remember it for later matches
      continue;
    }

    //only pure, non-memory instructions are safe to CSE within a single block
    if(I.isTerminator() || isa<PHINode>(&I) || isa<AllocaInst>(&I) ||
       I.mayReadOrWriteMemory() || I.mayHaveSideEffects() || I.isEHPad())
      continue;

    auto H = hash_combine(I.getOpcode(), I.getType(), hash_combine_range(I.value_op_begin(), I.value_op_end()));

    auto &Bucket = ExprTable[H];//candidate instructions that hash the same as I
    Instruction *Match = nullptr;

    for(Instruction *Earlier : Bucket) {
      if(Earlier->isIdenticalTo(&I)) {//same opcode, operands, type and flags: a true common subexpression
        Match = Earlier;
        break;
      }
    }

    if(Match) {
      errs() << "CSE: replacing " << I << " with earlier identical " << *Match << "\n";
      I.replaceAllUsesWith(Match);//reuse the earlier computed value
      I.eraseFromParent();//this recomputation is now dead, remove it from the block
      changed = true;
      continue;
    }

    Bucket.push_back(&I);//first time we've seen this expression in the block, remember it for later matches
  }

  return changed;
}

bool nanPerformLocalOpt(BasicBlock &BB)
{
  bool changed = false;

  errs() << "optimizing BB " << BB.getName() << " of function " << BB.getParent()->getName() << "\n";

  //each sub-optimization can expose new opportunities for the others (e.g. an algebraic
  //identity turning a store's value into a constant lets propagation see it), so iterate
  //this local pipeline to a fixpoint instead of running each piece just once
  bool localChanged;
  do {
    localChanged = false;
    localChanged |= nanPropagateAllocaConstants(BB);//propagate constants stored into pointers to their later loads
    localChanged |= nanEliminateCommonSubexpressions(BB);//reuse earlier identical pure computations instead of recomputing them
    localChanged |= nanAlgebraicXopId(BB);//due to side effect produced by PROGRAMMER with no change in program semantics.
    localChanged |= nanConstantFold(BB);//fold binary ops whose operands are both constant ints
    changed |= localChanged;
  } while(localChanged);

  return changed;
}

bool nanEliminateDeadCode(Function &F)
{
  bool changed = false;

  // Section 1: remove blocks that can never be reached from the entry block.
  {
    SmallPtrSet<BasicBlock *, 8> Reachable;
    SmallVector<BasicBlock *, 8> Worklist;
    Reachable.insert(&F.getEntryBlock());
    Worklist.push_back(&F.getEntryBlock());

    while(!Worklist.empty()) {
      auto *BB = Worklist.pop_back_val();
      for(auto *Succ : successors(BB)) {
        if(Reachable.insert(Succ).second)//first time reaching this successor
          Worklist.push_back(Succ);
      }
    }

    for(BasicBlock &BB : make_early_inc_range(F)) {
      if(Reachable.count(&BB))
        continue;

      errs() << "Removing unreachable block " << BB.getName() << "\n";
      BB.dropAllReferences();//drop uses first, so other dead blocks referring to this one can still be erased safely
      changed = true;
    }

    for(BasicBlock &BB : make_early_inc_range(F)) {
      if(!Reachable.count(&BB))
        BB.eraseFromParent();
    }
  }

  // Section 2: within each remaining block, remove dead results and dead (overwritten-or-never-read) stores.
  // This has to run to a fixpoint: removing a dead load can make its address's store dead, and
  // removing a dead store can make the pointer's now-sole-use alloca dead in turn.
  bool sectionChanged;
  do {
    sectionChanged = false;

    for(BasicBlock &BB : F) {
      DenseMap<Value *, StoreInst *> LastStore;//pointer -> most recent store to it that hasn't been read since

      for(Instruction &I : make_early_inc_range(BB)) {//iterate over all instructions in the block; I may be erased, so advance the iterator first
        if(isa<CallInst>(&I)) {
          LastStore.clear();//a call may read through a pointer we can't see, its prior store might be observed
          continue;
        }

        if(auto *LI = dyn_cast<LoadInst>(&I)) {
          if(LI->use_empty() && !LI->isVolatile()) {
            errs() << "Removing dead load " << *LI << ", it has no uses\n";
            LI->eraseFromParent();//loaded value is never used, e.g. the computation using it folded away to a constant
            sectionChanged = true;
            continue;
          }

          LastStore.erase(LI->getPointerOperand());//this pointer has now been read, its last store is no longer dead
          continue;
        }

        if(auto *SI = dyn_cast<StoreInst>(&I)) {
          auto *Ptr = SI->getPointerOperand();
          auto It = LastStore.find(Ptr);
          if(It != LastStore.end() && !It->second->isVolatile() && !SI->isVolatile()) {
            errs() << "Removing dead store " << *It->second << ", overwritten before being read\n";
            It->second->eraseFromParent();//nothing read this value before it was overwritten
            sectionChanged = true;
          }

          LastStore[Ptr] = SI;//track this store as the newest write to the pointer
          continue;
        }

        if(I.isTerminator() || isa<PHINode>(&I) || I.mayHaveSideEffects())
          continue;

        if(I.use_empty()) {
          errs() << "Removing dead result " << I << ", it has no uses\n";
          I.eraseFromParent();//pure computation whose result is never used
          sectionChanged = true;
        }
      }

      // A store still pending here was never read before the block returns, so if it's writing to a
      // local alloca that isn't used anywhere else, the value is never observed: the store is dead.
      if(isa<ReturnInst>(BB.getTerminator())) {
        for(auto &Entry : LastStore) {
          auto *AI = dyn_cast<AllocaInst>(Entry.first);
          if(!AI || Entry.second->isVolatile())
            continue;

          if(!all_of(AI->users(), [&](User *U) { return U == Entry.second || isa<LoadInst>(U); }))
            continue;//alloca escapes (e.g. passed to a call) or is used some other way we can't reason about here

          errs() << "Removing dead store " << *Entry.second << ", value is never read before the function returns\n";
          Entry.second->eraseFromParent();
          sectionChanged = true;
        }
      }
    }

    changed |= sectionChanged;
  } while(sectionChanged);

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

  // PromotePass().run(F, AM);//promote allocas to SSA registers (mem2reg) so identity checks can see repeated uses of the same value

  for(BasicBlock &BB : F) {//iterate over all basic blocks in the function
    dumpBasicBlock(BB, "BEFORE OPT");
    nanPerformLocalOpt(BB);//Optimization local to basic block: XopID and strength reduction.
    dumpBasicBlock(BB, "AFTER  OPT");
  }

  nanEliminateDeadCode(F);//remove unreachable blocks, then dead results and dead stores left behind by the above

  for(BasicBlock &BB : F) {//blocks may have been removed above, so re-iterate what's left
    dumpBasicBlock(BB, "AFTER  DCE");
  }

  return PreservedAnalyses::all();
}

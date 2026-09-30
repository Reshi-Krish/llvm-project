# Reshi Pass: Local Optimizations in LLVM

This coursework adds a function pass, `reshi-pass`, to LLVM. It implements these optimizations:

| # | Optimization | What it does | Function in the source |
|---|---|---|---|
| 1 | Constant propagation | A load from a pointer that last had a constant stored into it is replaced by that constant | `nanPropagateAllocaConstants` |
| 2 | Inst combine (algebraic identities + constant folding) | `x+0`, `x*1`, `x/1`, `x-x → 0`, `x/x → 1`, `x+x → x<<1`; binary ops on two constants are folded | `nanAlgebricXopIdInternal`, `nanConstantFoldInternal` |
| 3 | Dead code elimination | Removes unreachable blocks, unused results, and dead stores (overwritten before being read, or never read before return) | `nanEliminateDeadCode` |
| 4 | Strength reduction | Multiplication by a power of 2 becomes a left shift (`x*8 → x<<3`, `2*x → x<<1`) | `nanAlgebricXopIdInternal` (Mul case) |
| 5 | Common subexpression elimination | Reuses an earlier identical pure instruction, and removes redundant loads from the same pointer | `nanEliminateCommonSubexpressions` |

Optimizations 1, 2, 4 and 5 run on each basic block repeatedly until nothing changes, because each can expose new opportunities for the others. DCE then runs once over the whole function.

The pass prints a log to stderr describing every change it makes (for example `applying x * C = x << shift for : ...`), along with a dump of each block before and after optimization.

## Files

| File | Purpose |
|---|---|
| `llvm/lib/Transforms/Utils/HelloWorld.cpp` | **The full implementation, placed in the existing HelloWorld pass.** This is the only file needed to verify the work. |
| `llvm/lib/Transforms/Utils/ReshiPass.cpp` | The same implementation as a separate pass named `reshi-pass` |
| `llvm/include/llvm/Transforms/Utils/ReshiPass.h` | Header for `ReshiPass` |
| `llvm/lib/Transforms/Utils/CMakeLists.txt` | Adds `ReshiPass.cpp` to the build |
| `llvm/lib/Passes/PassRegistry.def`, `PassBuilder.cpp` | Register `reshi-pass` with `opt` |
| `experiments/test/*.c` | Test programs, one per optimization |
| `experiments/test/*_unopt.ll` | Unoptimized IR generated from each test program |
| `experiments/test/*_reshi_nom2r.ll` | Reference output of the pass for each test |

`HelloWorld.cpp` and `ReshiPass.cpp` contain the same code; only the class name differs.

## Option A (recommended): swap in the HelloWorld pass

This option needs no registration changes, and only two targets need to be rebuilt.

1. Replace `llvm/lib/Transforms/Utils/HelloWorld.cpp` in your LLVM tree with the `HelloWorld.cpp` from this submission. `HelloWorld.h` does not change.

2. Rebuild only the library that contains the pass, then `opt`:

   ```bash
   cd llvm-project/build
   ninja LLVMTransformUtils
   ninja opt
   ```

   A full LLVM build is not needed.

3. Run the pass under its existing name, `helloworld`:

   ```bash
   bin/opt -passes=helloworld -S ../experiments/test/ConstantProp2_unopt.ll -o ../experiments/test/ConstantProp2_out.ll
   ```

## Option B: use the full source tree as `reshi-pass`

If you build this whole tree, the pass is also registered as `reshi-pass`:

```bash
cd llvm-project/build
ninja LLVMTransformUtils LLVMPasses
ninja opt
bin/opt -passes=reshi-pass -S ../experiments/test/ConstantProp2_unopt.ll -o ../experiments/test/ConstantProp2_out.ll
```

## Running the tests

Run all commands from `llvm-project/build`. Replace `helloworld` with `reshi-pass` if you used Option B.

### Step 1: generate the unoptimized IR

```bash
clang -S -emit-llvm -Xclang -disable-O0-optnone ../experiments/test/<Test>.c -o ../experiments/test/<Test>_unopt.ll
```

> **Keep `-Xclang -disable-O0-optnone`.** Clang marks every function `optnone` at `-O0` by default. The pass is an optional pass, so LLVM skips it on `optnone` functions and the output would be unchanged.

The `_unopt.ll` files are already included in `experiments/test/`, so you can skip this step.

### Step 2: run the pass

```bash
bin/opt -passes=helloworld -S ../experiments/test/<Test>_unopt.ll -o ../experiments/test/<Test>_out.ll
```

### Step 3: check the result

Compare the function body in `<Test>_out.ll` with the expected result below, or diff it against the included reference output:

```bash
diff ../experiments/test/<Test>_out.ll ../experiments/test/<Test>_reshi_nom2r.ll
```

The `ModuleID` line in the first line of the file may differ; everything else should match.

### All tests at once

```bash
cd llvm-project/build
for t in ConstantProp2 Algebraic_1 ConstantFolding_1 SR_2 DCE_2; do
  clang -S -emit-llvm -Xclang -disable-O0-optnone ../experiments/test/$t.c -o ../experiments/test/${t}_unopt.ll
  bin/opt -passes=helloworld -S ../experiments/test/${t}_unopt.ll -o ../experiments/test/${t}_out.ll 2> ../experiments/test/${t}_log.txt
  echo "== $t"; sed -n '/^define/,/^}/p' ../experiments/test/${t}_out.ll
done
```

The optimized function body of each test is printed. The pass log for each test is saved to `<Test>_log.txt`.

## Test cases and expected results

Every test starts as `-O0` IR full of `alloca`/`store`/`load` instructions. After the pass, each one reduces to a single `ret` of a constant.

| Test | Tests | Source | Expected function body |
|---|---|---|---|
| `ConstantProp2.c` | Constant / copy propagation, CSE of loads | `b=4; c=b; e=c+b; return e;` | `ret i32 8` |
| `Algebraic_1.c` | Algebraic identities (inst combine) | `a/a`, `b/b`, `b-b`, `result/result`, `result-result`, then `+23` | `ret i32 23` |
| `ConstantFolding_1.c` | Constant folding | `c=4+a+b; result=(0+2+3)*9/2` | `ret i32 22` |
| `SR_2.c` | Strength reduction | `d=2*c; e=f*8;` | `ret i32 3` |
| `DCE_2.c` | Dead code elimination | redundant `a=3; b=4;`, unused `c=a+b` | `ret i32 3` |

### What to look for in the log

The final IR shows the combined effect. The stderr log shows each optimization applied individually:

| Optimization | Log lines to look for |
|---|---|
| Constant propagation | `Tracking constant store ...`, `Replacing load ... with constant ...` |
| Algebraic identities | `applying x / x = 1 ...`, `applying x - x = 0 ...`, `applying x +- ID = x ...` |
| Constant folding | `Constant folding ... into ...` |
| Strength reduction | `applying C * x = x << shift ...`, `applying x * C = x << shift ...` |
| CSE | `CSE: replacing redundant load ...`, `CSE: replacing ... with earlier identical ...` |
| DCE | `Removing dead store ...`, `Removing dead load ...`, `Removing dead result ...`, `Removing unreachable block ...` |

**Note on `SR_2`:** the shifts produced by strength reduction are unused in this program, so DCE removes them and the final IR is just `ret i32 3`. The strength reduction is visible in the log (`x * C = x << shift`) and in the `AFTER  OPT` block dump, which is printed before DCE runs. For a cleaner view, convert the IR to SSA form with `mem2reg` first, then inspect the `AFTER  OPT` dump:

```bash
bin/opt -passes=mem2reg -S ../experiments/test/SR_2_unopt.ll -o ../experiments/test/SR_2_ssa.ll
bin/opt -passes=helloworld -S ../experiments/test/SR_2_ssa.ll -o /dev/null 2>&1 | grep -A4 "AFTER  OPT"
```

The output contains `shl i32 undef, 1` and `shl i32 undef, 3` in place of the two multiplications.

## Scope and limitations

- Constant propagation and CSE are **local**: they work within a single basic block. Their tracking tables are reset at the end of each block.
- There is no alias analysis, so any `call` or `store` conservatively invalidates tracked loads and constants.
- Constant folding and algebraic identities handle integer binary operators only (no floating point).
- Division by zero and over-wide shifts are never folded.

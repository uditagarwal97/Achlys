/* ********* TaintChecker.cpp *************
 *
 * Taint Analysis for Achlys.
 * Author(s): Udit Kumar Agarwal
 *
 * Run this pass as: $ACHLYSLLVM/opt -enable-new-pm=0 \
 *      -load $ACHLYSLLVM/../lib/Achlys.so -adce -always-inline -argpromotion \
 *      -deadargelim  -inline -reg2mem -mergereturn -dse -taintanalysis -S \
 *       intraproc.ll > passOutput.ll
 *
 * -adce and -dse options are for removing deal code; -argpromotion pass promotes
 * pass-by-reference to pass-by-value, wherever possible; -reg2mem promotes
 * stack usage over heap usage; -mergereturn ensures that there's only one
 * exit basic block of the function; -always-inline and -inline passes are for
 * inlining functions, whenever possible.
 */

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace llvm;
using namespace std;

#define __DEBUG__

#include "TaintChecker.h"

namespace achlys {

  // Analyze each function one by one. We will use a lattice-based fixpoint
  // iterative, inter-procedural data flow analysis.
  // TODO:
  //    - Handle recursive function calls.
  //    - Handle pointers and data structures (Abstract memory model)
  void AchlysTaintChecker::analyzeFunction(Function *F, FunctionContext fc) {
    debug<<"Started Analyzing function: "<<F->getName().str().c_str()<<"\n";
    debug<<"Finished Analyzing function: "<<F->getName().str().c_str()<<"\n";
  }

  // Entry point of this pass.
  bool AchlysTaintChecker::runOnModule(Module& M) {

    debug<<"---------------Starting taint analysis pass-------------------\n";

    // Iterate through all function in the program and find the main() function.
    for (Function &F : M) {

      if (isUserDefinedFunction(F) && F.getName().str().compare("main") == 0) {

        // main() function takes 2 arguments or none
        if (F.arg_size() == 0) {
          funcWorklist.push({&F, {nullptr, nullptr, {0}}});
        }
        else if (F.arg_size() == 2) {
          funcWorklist.push({&F, {nullptr, nullptr, {1, 2}}});
        }
        else {
          assert(false && "Assuming that main() function takes either 0 or \
                           2 arguments");
        }
        break;
      }
    }

    if (funcWorklist.size() == 0) {
      output<<"Could not find main function! Aborting!\n";
      gracefulExit();
      return false;
    }
    else {

      while(funcWorklist.size() != 0) {
        auto elem = funcWorklist.front();
        Function *F = elem.first;
        FunctionContext fc = elem.second;

        funcWorklist.pop();

        analyzeFunction(F, fc);
        errs()<<"funcWorklist size="<<funcWorklist.size()<<"\n";
      }
    }

    gracefulExit();
    return false;
  }

  // Our taint analysis pass depends on these LLVM passes.
  void AchlysTaintChecker::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<MemoryDependenceWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesAll();
  }
} // namespace

char achlys::AchlysTaintChecker::ID = 0;
static RegisterPass<achlys::AchlysTaintChecker> X("taintanalysis",
                                   "Pass to find tainted variables");

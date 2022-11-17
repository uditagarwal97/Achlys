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
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/IntrinsicInst.h"

#include <cstdarg>

using namespace llvm;
using namespace std;

#define __DEBUG__
#define NARGS_SEQ(_1,_2,_3,_4,_5,_6,_7,_8,_9,N,...) N
#define NARGS(...) NARGS_SEQ(__VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1)
#define dprintf(a, ...) dprint(a, NARGS(__VA_ARGS__), __VA_ARGS__)

// CLI command to specify verbose number for debug statements.
static cl::opt<unsigned> Verbose("achlys-verbose", cl::init(0),
                                   cl::desc("[Achlys Taint Analysis] Debug \
                                              verbose value.\n\
                                              0 --> No Debug\n\
                                              1 --> Function Level Debug\n\
                                              2 --> BasicBlock Level Debug\n\
                                              3 --> Instruction Level Debug\n\
                                              4 --> Real-Time Debug\n"));

#include "TaintChecker.h"

namespace achlys {

  // Analyze each instruction one by one. Essentially, this function will apply
  // taint propogation and eviction policies on each instruction.
  void AchlysTaintChecker::analyzeInstruction(Instruction* i,
                          FunctionTaintSet* fc, FunctionContext fcxt,
                          TaintDepGraph* depGraph) {

    dprintf(3, "[STEP] Analyzing instruction: ",llvmToString(i).c_str(),"\n");

    // Handle Store Instruction.
    if (auto si = dyn_cast<StoreInst>(i)) {

      // What are you storing?
      auto valToStore = si->getOperand(0);
      // Where are you storing?
      auto storeLocation = si->getOperand(1);

      // [Taint Propogation] If what you are storing is tainted, then mark the
      // store location as tainted.
      fc->checkAndPropagateTaint(storeLocation, {valToStore});
      depGraph->checkAndPropogateTaint(storeLocation, {valToStore});

      // [Taint Eviction] If you are storing an untainted value to a tainted
      // location, then remove the taint of the location.
      // FIXME: Should we remove taint if this operation is in some aggregate DS?
      // For example, if you are storing an untainted val into a tainted array
      // should we remove the taint of the entire array? Perhaps, No. Need to
      // think more about the granularity at which we are tracking taints.
      if (fc->isTainted(storeLocation) && !(fc->isTainted(valToStore))) {
        fc->removeTaint(storeLocation);
        depGraph->removeTaint(storeLocation);
      }
    }

    // Handle Load Instruction.
    else if (auto li = dyn_cast<LoadInst>(i)) {

      // Where are you loading from?
      auto loadLocation = li->getOperand(0);

      // [Taint Propogation] If you are loading from a tainted location, mark
      // loaded value as tainted as well.
      fc->checkAndPropagateTaint(li, {loadLocation});
      depGraph->checkAndPropogateTaint(li, {loadLocation});
    }

    // Hanlde GetElementPointer (GEP) instruction. GEP is used for creating a
    // pointer from a variable.
    else if (auto gep = dyn_cast<GetElementPtrInst>(i)) {

      auto val = gep->getPointerOperand();

      // [Taint Propogation] If you are referencing a tainted variable, mark the
      // resulting pointer as tainted as well.
      fc->checkAndPropagateTaint(gep, {val});
      depGraph->checkAndPropogateTaint(gep, {val});
    }

    // Hanlde Binary operator like add, sub, mul, div, fdiv, etc.
    // Operation like div can produce NaNs as well, beware!
    else if (auto bo = dyn_cast<BinaryOperator>(i)) {

      auto firstOperand = bo->getOperand(0);
      auto secondOperand = bo->getOperand(1);
      auto opcode = bo->getOpcode();

      if (!isConstantInstruction(opcode, firstOperand, secondOperand)) {
        // [Taint Propogation] If any of the two operands is tainted, then the
        // resulting value will also be tainted.
        fc->checkAndPropagateTaint(bo, {firstOperand, secondOperand});
        depGraph->checkAndPropogateTaint(bo, {firstOperand, secondOperand});
      }

      // Check for NaN sources.
      // Instructions like a / b can produce NaN is a and b both are tainted.
      if ((opcode == Instruction::SDiv || opcode == Instruction::FDiv) &&
            fc->isUnconditionalTainted(secondOperand) &&
            fc->isUnconditionalTainted(firstOperand)) {

          fc->addNaNSource(bo);
          fc->checkAndPropagateTaint(bo, {secondOperand, firstOperand});
          depGraph->checkAndPropogateTaint(bo, {secondOperand, firstOperand});
      }
    }

    // Handle variable cast or unary operators line not.
    // Both these instruction types have just one operand.
    // FIXME: Can integer cast to float result in a NaN? IDK.
    else if (isa<CastInst>(i) || isa<UnaryOperator>(i)) {

      auto firstOperand = i->getOperand(0);
      fc->checkAndPropagateTaint(i, {firstOperand});
      depGraph->checkAndPropogateTaint(i, {firstOperand});
    }

    // Handle Call instruction.
    else if (auto ci = dyn_cast<CallInst>(i)) {

      // FIXME: Can we handle indirect function calls? For these, callee will be
      // NULL.
      if (Function* callee = ci->getCalledFunction()) {

        size_t numArguments = callee->arg_size();

        // If it is a user defined function, add it to the function worklist.
        if (isUserDefinedFunction(*callee)) {

          // Check which arguments of this call are tainted.
          vector<int> argTainted;
          vector<Value*> taintedVal;

          for (size_t i = 0; i < numArguments; i++) {

            auto arg = ci->getArgOperand(i);
            if (fc->isTainted(arg)) {
              argTainted.push_back(i+1);
              taintedVal.push_back(arg);
            }
          }

          if (argTainted.size() == 0)
            argTainted.push_back(0);

          funcWorklist.push({callee, {ci, ci->getParent()->getParent(),
                            argTainted}});

          // Check if this function returns some value. If yes, check if it could
          // be tainted.
          if (!callee->getReturnType()->isVoidTy()) {
            fc->taintFunctionReturnValue(ci, ci);
            depGraph->addCallSiteReturnTaint(ci, callee, taintedVal);
          }
        }
        // For third-party functions, we have to check if it is either a
        // taint source, or a function that can induce NaN (like atof()).
        // [Taint Popogation] If none of the above, we will assume that the
        // return value of this function is tainted if any of the input argument
        // is tainted.
        else if (isTaintSourceFunction(*callee)) {

          dprintf(1, "[New Info] Found new Taint source: ",
                      demangle(callee->getName().str().c_str()).c_str(), "\n");

          if (demangle(callee->getName().str().c_str()).find("istream")
              != string::npos) {
            Value* inpVal = ci->getArgOperand(numArguments - 1);
            fc->checkAndPropagateTaint(inpVal, {});
            depGraph->addTaintSource(inpVal);
          }
          else {
            fc->checkAndPropagateTaint(ci, {});
            depGraph->addTaintSource(ci);
          }
        }
        else {

          bool isArgTainted = false;
          vector<Value*> taintedARgs;
          for (size_t i = 0; i < numArguments; i++) {

            auto arg = ci->getArgOperand(i);
            if (fc->isTainted(arg)) {
              isArgTainted = true;
              taintedARgs.push_back(arg);
            }
          }

          if (isNaNSourceFunction(*callee)) {
            if (isArgTainted) {
              fc->addNaNSource(ci);
              fc->taintFunctionReturnValue(ci, {});
              depGraph->checkAndPropogateTaint(ci, taintedARgs);
            }
          }
          else {
            if (isArgTainted)
              fc->checkAndPropagateTaint(ci, {});
          }
        }
      }
    }

    // Hanlde return instruction.
    else if (auto ri = dyn_cast<ReturnInst>(i)) {

      if (!ri->getParent()->getParent()->getReturnType()->isVoidTy()) {

        Value *vl = ri->getOperand(0);
        fc->checkAndPropagateTaint(ri, {vl});
        depGraph->checkAndPropogateTaint(ri, {vl});
        depGraph->markReturnValue(ri);
      }

      fc->markThisValueAsReturnValue(ri);
    }
    else {
      dprintf(3, "\033[0;31m [WARNING] Unhandled Instruction: ",llvmToString(i).c_str(),
              " \033[0m\n");
    }

    dprintf(3, "[STEP] Finish Analyzing instruction: ", llvmToString(i).c_str(), "\n");
  }

  // Analyze each function one by one. We will use a lattice-based fixpoint
  // iterative, inter-procedural data flow analysis.
  // TODO:
  //    - Handle recursive function calls.
  //    - Handle pointers and data structures (Abstract memory model)
  //    - Handle back-tracking and updating taint sets of caller functions.
  void AchlysTaintChecker::analyzeFunction(Function *F, FunctionContext fc) {
    dprintf(1, "Started Analyzing function: ", F->getName().str().c_str(), "\n");

    FunctionTaintSet taintSet;
    TaintDepGraph* depGraph;

    if (funcTaintSet.find(F) == funcTaintSet.end()) {
      dprintf(1, "[STEP] Analysing this function for the first time\n");
      depGraph = new TaintDepGraph(F);
      funcTaintSummaryGraph.insert({F, depGraph});
    }
    else {
      dprintf(1, "[STEP] Analysing this function again\n");
      taintSet = funcTaintSet[F];
      taintSet.snapshot(); // Just to check if the taint set changes or not.
      depGraph = funcTaintSummaryGraph[F];
    }

    // Add function parameters to taint set.
    if (fc.numArgTainted[0] != 0){

      int argCounter = 0, idxCounter = 0;
      for (auto ii = F->arg_begin(); ii != F->arg_end(); ii++) {
        auto arg = ii;
        argCounter++;

        if (fc.numArgTainted[idxCounter] == argCounter) {

          if (F->getName().str().find("main") != string::npos)
            taintSet.checkAndPropagateTaint(arg, {});
          else
            taintSet.taintFunctionReturnValue(arg, arg);

          dprintf(1, "[New Info] Found tainted argument ",
                  llvmToString(arg).c_str(), "\n");

          // Remove the argument form the set.
          idxCounter++;
        }

        // Add all arguments to the dependency graph, irrespective of whether or
        // not they are tainted.
        depGraph->addFunctionArgument(arg);
      }
    }

    // Now, do the reverse post-order traversal of all the basic blocks of
    // this function
    for (BasicBlock *bb : llvm::ReversePostOrderTraversal<llvm::Function*>(F)) {
      for (auto ii = bb->begin(); ii != bb->end(); ii++) {

        Instruction* ins = &(*ii);
        analyzeInstruction(ins, &taintSet, fc, depGraph);
      }
    }

    taintSet.summarize(1);
    depGraph->printGraph(1);

    // FIXME: Complete this.
    if (taintSet.hasChanged) {
      dprintf(1, "[DEBUG] TaintSet of this function has changed. Back track and \
              reanalyze the callers if the return value is tainted\n");

      if ((!F->getReturnType()->isVoidTy()) &&
          taintSet.isReturnValueTainted.first) {

        dprintf(1, "[STEP] Return value of this function is tainted. \
                    Backtracking to re-analyze the caller\n");
      }
    }

    // Add the taint set of this function to the global map.
    if (funcTaintSet.find(F) == funcTaintSet.end()){
      funcTaintSet.insert({F, taintSet});
    }
    else {
      funcTaintSet[F] = taintSet;
    }

    dprintf(1, "[STEP] Finished Analyzing function: ",
                F->getName().str().c_str(), "\n");
  }

  // Entry point of this pass.
  bool AchlysTaintChecker::runOnModule(Module& M) {

    dprintf(1,
          "---------------Starting taint analysis pass-------------------\n");

    // Iterate through all function in the program and find the main() function.
    for (Function &F : M) {

      if (isUserDefinedFunction(F) && F.getName().str().compare("main") == 0) {

        //AAResultsWrapperPass a;
        //a.runOnFunction(F);
        //aliasAnalysisResult = &(a.getAAResults());
        // Init results from alias analysis pass for this function.
        aliasAnalysisResult = &(getAnalysis<AAResultsWrapperPass>(F).getAAResults());

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
        dprintf(1, "funcWorklist size=", to_string(funcWorklist.size()).c_str(),
               "\n");
      }
    }

    gracefulExit();
    return false;
  }

  // Our taint analysis pass depends on these LLVM passes.
  void AchlysTaintChecker::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<AAResultsWrapperPass>();
    AU.addRequired<MemoryDependenceWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
  }
} // namespace

char achlys::AchlysTaintChecker::ID = 0;
static RegisterPass<achlys::AchlysTaintChecker> X("taintanalysis",
                                   "Pass to find tainted variables", false, true);

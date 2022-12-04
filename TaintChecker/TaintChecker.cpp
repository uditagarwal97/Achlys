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
 * -adce and -dse options are for removing deal code; -argpromotion pass
 * promotes pass-by-reference to pass-by-value, wherever possible; -reg2mem
 * promotes stack usage over heap usage; -mergereturn ensures that there's only
 * one exit basic block of the function; -always-inline and -inline passes are
 * for inlining functions, whenever possible.
 */

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"

#include <cstdarg>
#include <ctime>
#include <unordered_set>

using namespace llvm;
using namespace std;

#define __DEBUG__
#define NARGS_SEQ(_1, _2, _3, _4, _5, _6, _7, _8, _9, N, ...) N
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

// CLI command to specify if fault injection is needed or not.
static cl::opt<bool> doFaultInjection("doFaultInjection", cl::init(false),
                                      cl::desc("[Achlys Taint Analysis] \
                                            Enable Fault Injection"));

#include "TaintChecker.h"

namespace achlys {

// Analyze each instruction one by one. Essentially, this function will apply
// taint propogation and eviction policies on each instruction.
void AchlysTaintChecker::analyzeInstruction(Instruction *i,
                                            FunctionTaintSet *taintSet,
                                            FunctionContext *fcxt,
                                            TaintDepGraph *depGraph,
                                            PtrMap *pointerMap) {

  dprintf(3, "[STEP] Analyzing instruction: ", llvmToString(i).c_str(), "\n");

  // Handle Store Instruction.
  if (auto si = dyn_cast<StoreInst>(i)) {

    // What are you storing?
    auto valToStore = si->getOperand(0);
    // Where are you storing?
    auto storeLocation = si->getOperand(1);

    if (isValidPtrType(valToStore) && isValidPtrType(storeLocation)) {
      if (!isa<Constant>(valToStore)) {
        // const Constant *CI = dyn_cast<Constant>(valToStore);
        // if (CI->isNullValue()) {
        //   dprintf(1, "[STORE_DEP] has null value: ",
        //           llvmToString(valToStore).c_str(), " \n");
        // }
        pointerMap->insert(storeLocation, valToStore);
      }
    }

    // [Taint Propogation] If what you are storing is tainted, then mark the
    // store location as tainted.
    taintSet->checkAndPropagateTaint(storeLocation, {valToStore});
    depGraph->checkAndPropagateTaint(storeLocation, {valToStore});

    // [Taint Eviction] If you are storing an untainted value to a tainted
    // location, then remove the taint of the location.
    // FIXME: Should we remove taint if this operation is in some aggregate DS?
    // For example, if you are storing an untainted val into a tainted array
    // should we remove the taint of the entire array? Perhaps, No. Need to
    // think more about the granularity at which we are tracking taints.
    if (taintSet->isTainted(storeLocation) && !(taintSet->isTainted(valToStore))) {
      taintSet->removeTaint(storeLocation);
      depGraph->removeTaint(storeLocation);
    }
  }

  // Handle Load Instruction.
  else if (auto li = dyn_cast<LoadInst>(i)) {

    // Where are you loading from?
    auto loadLocation = li->getOperand(0);

    // Check for mmemory dependence between this load and previous store
    // instructions that store something at this memory location.
    SmallVector<NonLocalDepResult, 4> NLDep;
    memDepResult->getNonLocalPointerDependency(li, NLDep);

    for (auto res : NLDep) {
      if (res.getResult().isClobber() || res.getResult().isDef()) {
        Instruction *ins = res.getResult().getInst();
        if (auto ssi = dyn_cast<StoreInst>(ins)) {

          // What are you storing?
          auto src_val = ssi->getOperand(0);
          auto des_val = ssi->getOperand(1);

          if (isValidPtrType(src_val) && isValidPtrType(des_val)) {
            if (!isa<Constant>(src_val)) {
              pointerMap->insert(des_val, src_val);
            }
          }

          taintSet->checkAndPropagateTaint(li, {src_val});
          depGraph->checkAndPropagateTaint(li, {src_val});
        }
      }
    }

    if (isValidPtrType(loadLocation)) {
      pointerMap->insert(li, loadLocation);
    }

    // [Taint Propogation] If you are loading from a tainted location, mark
    // loaded value as tainted as well.
    taintSet->checkAndPropagateTaint(li, {loadLocation});
    depGraph->checkAndPropagateTaint(li, {loadLocation});
  }

  // Hanlde GetElementPointer (GEP) instruction. GEP is used for creating a
  // pointer from a variable.
  else if (auto gep = dyn_cast<GetElementPtrInst>(i)) {

    auto val = gep->getPointerOperand();

    // [Taint Propogation] If you are referencing a tainted variable, mark the
    // resulting pointer as tainted as well.
    taintSet->checkAndPropagateTaint(gep, {val});
    depGraph->checkAndPropagateTaint(gep, {val});

    pointerMap->insert(gep, val);
    if (pointerMap->pointerSet.find(val) != pointerMap->pointerSet.end()) {
      auto parents = pointerMap->pointerSet[val];

      for (auto parent : parents) {
        for (auto ii = pointerMap->pointerSet.begin();
             ii != pointerMap->pointerSet.end(); ++ii) {
          if (std::find(ii->second.begin(), ii->second.end(), parent) !=
              ii->second.end()) {
            taintSet->checkAndPropagateTaint(gep, {ii->first});
            depGraph->checkAndPropagateTaint(gep, {ii->first});
          }
        }
      }
    }
  }

  // Handle Phi Node. Phi node is used for merging values from different
  // branches.
  else if (auto phi = dyn_cast<PHINode>(i)) {
    // [Taint Propogation] If any of the incoming values are tainted, mark the
    // phi node as tainted as well.
    vector<Value *> incomingValues;
    for (unsigned i = 0; i < phi->getNumIncomingValues(); i++) {
      incomingValues.push_back(phi->getIncomingValue(i));
    }
    taintSet->checkAndPropagateTaint(phi, incomingValues);
    depGraph->checkAndPropagateTaint(phi, incomingValues);
  }

  // Handle Binary operator like add, sub, mul, div, fdiv, etc.
  // Operation like div can produce NaNs as well, beware!
  else if (auto bo = dyn_cast<BinaryOperator>(i)) {

    auto firstOperand = bo->getOperand(0);
    auto secondOperand = bo->getOperand(1);
    auto opcode = bo->getOpcode();

    if (!isConstantInstruction(opcode, firstOperand, secondOperand)) {
      // [Taint Propogation] If any of the two operands is tainted, then the
      // resulting value will also be tainted.
      taintSet->checkAndPropagateTaint(bo, {firstOperand, secondOperand});
      depGraph->checkAndPropagateTaint(bo, {firstOperand, secondOperand});
    }

    // Check for NaN sources.
    // Instructions like a / b can produce NaN is a and b both are tainted.
    if ((opcode == Instruction::SDiv || opcode == Instruction::FDiv) &&
        (depGraph->isTainted(secondOperand) ||
        depGraph->isTainted(firstOperand))) {

      taintSet->addNaNSource(bo);
      taintSet->checkAndPropagateTaint(bo, {secondOperand, firstOperand});
      depGraph->checkAndPropagateTaint(bo, {secondOperand, firstOperand});
      depGraph->markValueAsNaNSource(bo);
    }
  }

  // Handle variable cast or unary operators line not.
  // Both these instruction types have just one operand.
  // FIXME: Can integer cast to float result in a NaN? IDK.
  else if (isa<CastInst>(i) || isa<UnaryOperator>(i)) {

    auto firstOperand = i->getOperand(0);
    taintSet->checkAndPropagateTaint(i, {firstOperand});
    depGraph->checkAndPropagateTaint(i, {firstOperand});

    // Add to MemDep Graph, if feasible.
    pointerMap->insert(i, firstOperand);
  }

  // Hanlde comparision instruction.
  else if (auto ci = dyn_cast<CmpInst>(i)) {

    auto firstOperand = ci->getOperand(0);
    auto secondOperand = ci->getOperand(1);
    taintSet->checkAndPropagateTaint(i, {firstOperand, secondOperand});
    depGraph->checkAndPropagateTaint(i, {firstOperand, secondOperand});
  }

  // Handle Call instruction.
  else if (auto ci = dyn_cast<CallInst>(i)) {

    // FIXME: Can we handle indirect function calls? For these, callee will be
    // NULL.
    if (Function *callee = ci->getCalledFunction()) {

      size_t numArguments = callee->arg_size();

      // If it is a user defined function, add it to the function worklist.
      if (isUserDefinedFunction(*callee)) {

        // Check which arguments of this call are tainted.
        vector<int> argTainted;
        vector<Value *> taintedVal;

        for (size_t i = 0; i < numArguments; i++) {

          auto arg = ci->getArgOperand(i);
          if (taintSet->isTainted(arg)) {
            argTainted.push_back(i + 1);
            taintedVal.push_back(arg);
          }
        }

        if (argTainted.size() == 0)
          argTainted.push_back(0);

        funcWorklist.push(
          {callee, {ci, ci->getParent()->getParent(), argTainted}});

        // Check if this function returns some value. If yes, check if it could
        // be tainted.
        if (!callee->getReturnType()->isVoidTy()) {
          taintSet->taintFunctionReturnValue(ci, ci);
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

        if (demangle(callee->getName().str().c_str()).find("istream") !=
            string::npos) {
          Value *inpVal = ci->getArgOperand(numArguments - 1);
          taintSet->checkAndPropagateTaint(inpVal, {});
          depGraph->addTaintSource(inpVal);

          // cin can take float from user. So, it can produce NaN.
          if (inpVal->getType() == Type::getFloatPtrTy(i->getContext())) {

            depGraph->markValueAsNaNSource(inpVal, true);
          }
        } else {
          taintSet->checkAndPropagateTaint(ci, {});
          depGraph->addTaintSource(ci);
        }
      }
      // Check if this function is a heap allocator like malloc() etc.
      else if (isHeapAllocator(*callee)) {
        pointerMap->ptrTree->addToTop(ci);
        pointerMap->insert(ci, NULL);
      } else {

        bool isArgTainted = false;
        vector<Value *> taintedARgs;
        for (size_t i = 0; i < numArguments; i++) {

          auto arg = ci->getArgOperand(i);
          if (taintSet->isTainted(arg)) {
            isArgTainted = true;
            taintedARgs.push_back(arg);
          }
        }

        if (isNaNSourceFunction(*callee)) {
          if (isArgTainted) {
            taintSet->addNaNSource(ci);
            taintSet->taintFunctionReturnValue(ci, {});
            depGraph->checkAndPropagateTaint(ci, taintedARgs);
            depGraph->markValueAsNaNSource(ci);
          }
        } else {
          if (isArgTainted) {
            taintSet->checkAndPropagateTaint(ci, {});
            depGraph->checkAndPropagateTaint(ci, taintedARgs);
          }
        }
      }
    }
  }

  // Hanlde return instruction.
  else if (auto ri = dyn_cast<ReturnInst>(i)) {

    if (!ri->getParent()->getParent()->getReturnType()->isVoidTy()) {

      Value *vl = ri->getOperand(0);
      fcxt->retVal = vl;
      taintSet->checkAndPropagateTaint(ri, {vl});
      depGraph->checkAndPropagateTaint(ri, {vl});
      depGraph->markReturnValue(ri);
    }

    taintSet->markThisValueAsReturnValue(ri);
  }

  // Hanlde pointer allocation
  else if (auto alloc_inst = dyn_cast<AllocaInst>(i)) {

    if (alloc_inst->getAllocatedType()->isPointerTy() ||
        alloc_inst->getAllocatedType()->isArrayTy() ||
        alloc_inst->getAllocatedType()->isStructTy()) {

      pointerMap->ptrTree->addToTop(alloc_inst);
      pointerMap->insert(alloc_inst, NULL);
    }

  } else {
    dprintf(3, "\033[0;31m [WARNING] Unhandled Instruction: ",
            llvmToString(i).c_str(), " \033[0m\n");
  }

  dprintf(3, "[STEP] Finish Analyzing instruction: ", llvmToString(i).c_str(),
          "\n");
}

//  Analyzes instructions in a single basic block
void AchlysTaintChecker::analyzeBasicBlock(BasicBlock *bb,
                                           FunctionTaintSet *taintSet,
                                           FunctionContext *fc,
                                           TaintDepGraph *depGraph,
                                           PtrMap *pointerMap) {
  dprintf(2, "[BasicBlock] Analyzing Basic Block ", bb->getName().data(), "\n");
  for (auto ii = bb->begin(); ii != bb->end(); ii++) {
    Instruction *ins = &(*ii);
    analyzeInstruction(ins, taintSet, fc, depGraph, pointerMap);
  }
}

// Recursively analyzes basic block till taintSet does not change
void AchlysTaintChecker::analyzeLoop(BasicBlock *bb, FunctionTaintSet *taintSet,
                                     FunctionContext *fc, LoopInfo &loopInfo,
                                     TaintDepGraph *depGraph,
                                     PtrMap *pointerMap) {
  unsigned int depth = loopInfo.getLoopDepth(bb);
  taintSet->trackNewLoop();
  dprintf(2, "\033[0;33m[LOOP] Started analyzing Loop starting with \033[0m",
          bb->getName().data(), " with a depth of ", to_string(depth).c_str(),
          "\n");
  int loopUnrollCount = 0;
  while (taintSet->getCurrentLoopTaintsChanged()) {
    taintSet->summarize(4);
    ++loopUnrollCount;
    dprintf(2, "\033[0;33m[LOOP] Unroll Count: \033[0m",
            to_string(loopUnrollCount).c_str(), " for depth of ",
            to_string(depth).c_str(), "\n");

    taintSet->resetCurrentLoopTaintsChanged();
    BasicBlock *currBlock = bb;
    unsigned int loopUnrolls = 0;
    while (currBlock != nullptr && loopInfo.getLoopDepth(currBlock) >= depth) {
      if (loopInfo.getLoopDepth(currBlock) == depth)
        analyzeBasicBlock(currBlock, taintSet, fc, depGraph, pointerMap);
      else if (loopInfo.isLoopHeader(currBlock) == true)
        analyzeLoop(currBlock, taintSet, fc, loopInfo, depGraph, pointerMap);
      else
        dprintf(2, "[SKIP] Skipping Basic Block ", currBlock->getName().data(),
                " with depth ",
                to_string(loopInfo.getLoopDepth(currBlock)).c_str(), "\n");
      currBlock = currBlock->getNextNode();
    }
  }
  taintSet->finishTrackingLoop();
  dprintf(2, to_string(taintSet->loopTaintsChanged.size()).c_str(), "\n");
  dprintf(2, "\033[0;33m[LOOP] Finished analyzing Loop starting with  \033[0m",
          bb->getName().data(), " with a depth of ", to_string(depth).c_str(),
          "\n");
}

// Analyze each function one by one. We will use a lattice-based fixpoint
// iterative, inter-procedural data flow analysis.
// TODO:
//    - Handle recursive function calls.
//    - Handle pointers and data structures (Abstract memory model)
//    - Handle back-tracking and updating taint sets of caller functions.
void AchlysTaintChecker::analyzeFunction(Function *F, FunctionContext *fc,
                                        bool forceRecalculateSummary = false) {
  dprintf(1, "[STEP] Started Analyzing function: ",
          demangle(F->getName().str().c_str()).c_str());

  FunctionTaintSet taintSet;
  TaintDepGraph *depGraph;
  PtrMap *pointerMap;

  // Init results from alias analysis pass for this function.
  getAnalysis<AAResultsWrapperPass>(*F).doInitialization(*(F->getParent()));
  aliasAnalysisResult = &(getAnalysis<AAResultsWrapperPass>(*F).getAAResults());
  memDepResult = &(getAnalysis<MemoryDependenceWrapperPass>(*F).getMemDep());

  if (funcTaintSet.find(F) == funcTaintSet.end()) {
    dprintf(1, " for the first time\n");
    depGraph = new TaintDepGraph(F);
    funcTaintSummaryGraph.insert({F, depGraph});

    pointerMap = new PtrMap(F);
    funcPointerMap.insert({F, pointerMap});
  } else {
    dprintf(1, " again. Returning.\n");
    taintSet = funcTaintSet[F];
    taintSet.snapshot(); // Just to check if the taint set changes or not.
    depGraph = funcTaintSummaryGraph[F];

    pointerMap = funcPointerMap[F];

    if (!forceRecalculateSummary)
      return; // Don't calculate the function summary again.
  }

  // Add function parameters to taint set.

  int argCounter = 0, idxCounter = 0;
  for (auto ii = F->arg_begin(); ii != F->arg_end(); ii++) {
    auto arg = ii;
    argCounter++;

    if (fc->numArgTainted[0] != 0) {
      if (fc->numArgTainted[idxCounter] == argCounter) {

        if (F->getName().str().find("main") != string::npos)
          taintSet.checkAndPropagateTaint(arg, {});
        else
          taintSet.taintFunctionReturnValue(arg, arg);

        dprintf(1, "[NEW INFO] Found tainted argument ",
                llvmToString(arg).c_str(), "\n");

        // Remove the argument form the set.
        idxCounter++;
      }
    }

    // Add all arguments to the dependency graph, irrespective of whether or
    // not they are tainted.
    depGraph->addFunctionArgument(arg, argCounter);
    // TODO: if an arg is a pointer, add it as a base pointer in the PtrTree
    // TODO: not sure if above^ should be added, need to have a benchmark that
    // have multiple function call to see if args in each function would
    // become an alloca inst in .ll file
  }

  LoopInfo &loopInfo = getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo();

  // Now, do the reverse post-order traversal of all the basic blocks of
  // this function
  for (BasicBlock *bb : llvm::ReversePostOrderTraversal<llvm::Function *>(F)) {

    // If not a loop => analzye basic block
    if (loopInfo.getLoopDepth(bb) == 0) {
      dprintf(2, "[NON_LOOP] Non-loop basic block ", bb->getName().data(),
              "\n");
      analyzeBasicBlock(bb, &taintSet, fc, depGraph, pointerMap);
    }
    // Should we check for loopHeader? loopInfo.isLoopHeader(bb)==true
    else if (loopInfo.isLoopHeader(bb) == true &&
             loopInfo.getLoopDepth(bb) == 1) {
      analyzeLoop(bb, &taintSet, fc, loopInfo, depGraph, pointerMap);
    } else {
      dprintf(2, "[SKIP] Already Analyzed Basic Block ", bb->getName().data(),
              " with depth ", to_string(loopInfo.getLoopDepth(bb)).c_str(),
              "\n");
    }
  }

  // It's a hack to prevent duplicate sin the memory dependency graph.
  if (!forceRecalculateSummary){
    pointerMap->constructTree();
    depGraph->mergeMemDepGraph(pointerMap->ptrTree);
  }

  analyzeControlFlow(F, fc, &taintSet, depGraph);

  pointerMap->printTree(2);
  taintSet.summarize(4);
  depGraph->printGraph(2);

  // Add the taint set of this function to the global map.
  if (funcTaintSet.find(F) == funcTaintSet.end()) {
    funcTaintSet.insert({F, taintSet});
  } else {
    funcTaintSet[F] = taintSet;
  }

  dprintf(1, "[STEP] Finished Analyzing function: ",
          demangle(F->getName().str().c_str()).c_str(), "\n");
}

void AchlysTaintChecker::analyzeControlFlow(Function *F, FunctionContext *fc,
                          FunctionTaintSet *taintSet, TaintDepGraph *depGraph){

  // Get basic blocks in depTree
  if(fc->retVal != nullptr){
    dprintf(4, "===============================================\n",
            "Analyzing Control Flow for function with return value ", llvmToString(fc->retVal).c_str(),
            "\n===============================================\n", "\n");

    DominatorTree &domTree = getAnalysis<DominatorTreeWrapperPass>(*F).getDomTree();

    if (auto phi = dyn_cast<PHINode>(fc->retVal)){

      unsigned int numIncoming = phi->getNumIncomingValues();
      set<BasicBlock*> incomingBBs;

      // Analyze control flow for each incoming value
      for (unsigned int i = 0; i < numIncoming; i++){

        Value *incomingVal = phi->getIncomingValue(i);
        dprintf(4, "Incoming Value: ", llvmToString(incomingVal).c_str(),
        " from basic block ", phi->getIncomingBlock(i)->getName().data(), "\n");
        incomingBBs.insert(phi->getIncomingBlock(i));
      }
      for (auto bb1 = incomingBBs.begin(); bb1 != incomingBBs.end(); ++bb1){
        for (auto bb2 = next(bb1, 1); bb2 != incomingBBs.end(); ++bb2){
          BasicBlock* dominator = domTree.findNearestCommonDominator(*bb1, *bb2);
          // llvm::DominatorTree::findNearestCommonDominator(llvm::BasicBlock* const*, llvm::BasicBlock* const*)’
          //errs()<<"Found dominator "<< *dominator<<"\n";
          if (Instruction* ti = dyn_cast<BranchInst>(dominator->getTerminator())){

            auto cmpCond = ti->getOperand(0);
            taintSet->checkAndPropagateTaint(fc->retVal, {cmpCond});
            depGraph->checkAndPropagateTaint(fc->retVal, {cmpCond});
          }
        }
      }
    }
    else if(auto li = dyn_cast<LoadInst>(fc->retVal)){
      assert(false && "Not implemented for load yet");
    }
  }
}
// Stage 3 - Filter attacker controlled NaN nodes to keep only those that affects the
// control-flow of the program.
void AchlysTaintChecker::filterAttackerControlledNANSources(
    AttackerControlledNAN *attackCtrlNAN) {

  for (auto ii = attackCtrlNAN->nodeToFunctionMap.begin();
       ii != attackCtrlNAN->nodeToFunctionMap.end();) {

    auto it = &*ii;
    TaintDepGraphNode *node = it->first;
    Function *F = it->second;

    if (funcTaintSummaryGraph.find(F) != funcTaintSummaryGraph.end()) {

      TaintDepGraph *depGraph = funcTaintSummaryGraph[F];
      bool isValidNaNSource = false;

      // Check if any child of this node is a cmp instruction.
      // If yes, then check whether the cmp instruction in a branch instruction.
      int nanId = node->attr.nanSourceNumber;

      // If the node is a top-level node, just iterate through its child.
      if (depGraph->isTopLevelNode(node)) {
        // Iterate through children to find cmp instructions.
        for (auto child : node->children) {
          if (isa<CmpInst>(child->val) &&
              find(child->attr.derivedNaNSourceId.begin(),
                   child->attr.derivedNaNSourceId.end(),
                   nanId) != child->attr.derivedNaNSourceId.end()) {

            // Check if this compare instruction is used in a branch isntruction
            // or not.
            for (auto user : child->val->users()) {
              if (isa<BranchInst>(user)) {
                isValidNaNSource = true;
              }
            }
          }
        }
      }
      // If this NaN source is a child node, then iterate through all of its
      // parents.
      else {
        for (auto parent : node->children) {
          for (auto child : parent->children) {
            // Iterate through all child of all parents to find all cmp
            // instructions that directly or indirectly depends on this NaN
            // source.

            if (isa<CmpInst>(child->val) &&
                find(child->attr.derivedNaNSourceId.begin(),
                     child->attr.derivedNaNSourceId.end(),
                     nanId) != child->attr.derivedNaNSourceId.end()) {

              // Check if this compare instruction is used in a branch
              // isntruction or not.
              for (auto user : child->val->users()) {
                if (isa<BranchInst>(user)) {
                  isValidNaNSource = true;
                }
              }
            }
          }
        }
      }

      // Do an exhaustive search over all the nodes in the graph to find if
      // there's any CMP instruction with this derviedNaNSourceId.
      if (!isValidNaNSource) {
        for (auto ii = depGraph->valToNodeMap.begin();
              ii != depGraph->valToNodeMap.end(); ii++) {

          TaintDepGraphNode *node = ii->second;
          if (isa<CmpInst>(node->val) && find(node->attr.derivedNaNSourceId.begin(),
                                              node->attr.derivedNaNSourceId.end(),
                                              nanId) != node->attr.derivedNaNSourceId.end()) {

            isValidNaNSource = true;
            break;
          }
        }
      }

      if (!isValidNaNSource) {
        dprintf(1, "[NEW INFO] Removing attacker controlled NaN source: ",
                llvmToString(node->val).c_str(), "\n");
        ii = attackCtrlNAN->nodeToFunctionMap.erase(ii);
      } else {
        dprintf(1, "[NEW INFO] Keeping attacker controlled NaN source: ",
                llvmToString(node->val).c_str(), "\n");
        ii++;
      }
    } else
      assert(false && "Function summary graph not found while \
                filtering attacker controlled NaN sources");
  }
}

// Function to interprocedurally collapse constaints.
// It will return true if the return value of this function is tainted.
bool AchlysTaintChecker::collapseConstraints(
    Function *f, FunctionCallStack *funCS, vector<int> taintedArgs,
    AttackerControlledNAN *attackCtrlNAN) {

  dprintf(1, "[STEP] Started collapsing constraints for function: ",
          demangle(f->getName().str().c_str()).c_str(), "\n");

  // Check and get dependencies graph of this function.
  if (funcTaintSummaryGraph.find(f) == funcTaintSummaryGraph.end()) {
    dprintf(1, "[WARNING] No taint summary graph found for function: ",
            demangle(f->getName().str().c_str()).c_str(), "\n");
    return false;
  }

  TaintDepGraph *depGraph = funcTaintSummaryGraph[f];

  // Check if it is a recursion.
  // FIXME: Support recursive functions.
  if (funCS->isRecursion(f)) {
    dprintf(1, "[WARNING] Recursive function call found for function: ",
            demangle(f->getName().str().c_str()).c_str(), "\n");
    return false;
  }

  // Got followint tainted Args
  for (auto i : taintedArgs) {
    dprintf(4, "[DEBUG] Tainted argument: ", to_string(i).c_str(), "\n");
  }

  // Add it to the call stack.
  funCS->push(f);

  bool isRetTainted = false;
  unordered_map<TaintDepGraphNode *, int> nanSourceToTaintedParentCount;

  // Analyze all function-argument-based or 100% taint sources top-level taint
  // nodes and collapse the constraints.
  for (auto tlNode : depGraph->topLevelNodes) {
    if (tlNode->attr.isArgumentNode ||
        tlNode->type == TaintDepGraphNode::NodeType::DEF_TAINT_SOURCE) {

      // Find which function arguments are 100% tainted in the given
      // function call stack.
      if (find(taintedArgs.begin(), taintedArgs.end(),
               tlNode->attr.argumentNumber) != taintedArgs.end() ||
          tlNode->type == TaintDepGraphNode::NodeType::DEF_TAINT_SOURCE) {

        dprintf(1,
                "[STEP] Found tainted argument or definitly a taint source: ",
                to_string(tlNode->attr.argumentNumber).c_str(), "\n");

        tlNode->setCurrentStackTaint();

        if (tlNode->nanStatus == TaintDepGraphNode::NodeNaNStatus::NAN_SOURCE) {
          // If this is a NaN source, then add it to the attacker controlled
          // NaN sources.
          attackCtrlNAN->nodeToFunctionMap[tlNode] = f;
        }

        // Mark all child nodes as tainted.
        for (auto childNode : tlNode->children) {

          childNode->setCurrentStackTaint();
          if (childNode->isNaNSource()) {

            // Check if this node is already tainted by some other parent.
            if (nanSourceToTaintedParentCount.find(childNode) ==
                nanSourceToTaintedParentCount.end()) {
              nanSourceToTaintedParentCount.insert({childNode, 1});
              ;
            } else {
              nanSourceToTaintedParentCount[childNode]++;
            }
          }
          // If this node is 100% tainted and it is a return value.
          if (childNode->attr.isReturnValue) {
            isRetTainted = true;
          }
        }
      }
    }
  }

  // Analyze all call-site based top-level taint nodes.
  // We will resolve each call site in the same order as they appear in the
  // function source code. This order is implicitly maintained by the vector
  // callSiteReturnNode.
  for (auto tlNode : depGraph->callSiteReturnNode) {

    // Get call instruction.
    CallInst *callInst = dyn_cast<CallInst>(tlNode->val);

    // Get the called function.
    Function *calledFunc = callInst->getCalledFunction();

    // inter-procedurally collapse contraints for user-defined functions only.
    if (!isUserDefinedFunction(*calledFunc))
      continue;

    // Get all tainted arguments of the call instruction.
    vector<int> taintedCallArgNumber;
    int numArgument = 0;

    dprintf(3, "[STEP] Analysing call-site during collapse constraints: ",
            llvmToString(callInst).c_str(), "\n");

    for (auto ii = callInst->arg_begin(); ii != callInst->arg_end(); ii++) {
      numArgument++;

      auto arg_ii = ii;
      Value *arg = dyn_cast<Value>(arg_ii);

      // Check if this argument is a constant.
      if (isa<Constant>(arg)) {
        dprintf(3, "Found constant argument: ", llvmToString(arg).c_str(),
                "\n");
        continue;
      }

      if (depGraph->valToNodeMap.find(arg) != depGraph->valToNodeMap.end()) {
        TaintDepGraphNode *argNode = depGraph->valToNodeMap[arg];

        if (argNode->isNodeTaintedInCurrentStack()) {
          taintedCallArgNumber.push_back(numArgument);
        }
      } else {
        dprintf(1, "[WARNING] Value not found in the taint summary graph: ",
              llvmToString(arg).c_str(), "\n")
        //assert(false && "Didn't found the node in depGraph while collapse \
        //                 constraints");
      }
    }

    // Recursively collapse constraints.
    bool isRetValTainted = collapseConstraints(
        calledFunc, funCS, taintedCallArgNumber, attackCtrlNAN);

    if (isRetValTainted) {

      dprintf(1, "[STEP] Got tainted return value from function: ",
              demangle(calledFunc->getName().str().c_str()).c_str(), "\n");

      tlNode->setCurrentStackTaint();

      // Mark all child nodes as tainted.
      for (auto childNode : tlNode->children) {

        childNode->setCurrentStackTaint();
        if (childNode->isNaNSource()) {

          // Check if this node is already tainted by some other parent.
          if (nanSourceToTaintedParentCount.find(childNode) ==
              nanSourceToTaintedParentCount.end()) {
            nanSourceToTaintedParentCount.insert({childNode, 1});
            ;
          } else {
            nanSourceToTaintedParentCount[childNode]++;
          }
        }
        // If this node is 100% tainted and it is a return value.
        if (childNode->attr.isReturnValue) {
          isRetTainted = true;
        }
      }
    }
  }

  // Find nanSources with all parent tainted.
  for (auto nanSource : nanSourceToTaintedParentCount) {

    if (nanSource.second == nanSource.first->children.size()) {
      dprintf(1, "[NEW INFO] Found a nanSource with all parents tainted: ",
              llvmToString(nanSource.first->val).c_str(), "\n");

      TaintDepGraphNode *nanSourceNode = nanSource.first;
      attackCtrlNAN->addNode(nanSourceNode, f);
    }
    else {
      dprintf(1, "[DEBUG] Found a nanSource with some parents not tainted: ",
              llvmToString(nanSource.first->val).c_str(), "\n");

      Instruction* nanSourceType = dyn_cast<Instruction>(nanSource.first->val);

      // For floating point division, cehck if the numerator and demoninator
      // is tainted.
      if (isa<BinaryOperator>(nanSourceType) && (nanSourceType->getOpcode() == Instruction::FDiv ||
          nanSourceType->getOpcode() == Instruction::SDiv)) {

        auto numerator = nanSourceType->getOperand(0);
        auto denominator = nanSourceType->getOperand(1);

        if (depGraph->valToNodeMap.find(numerator) != depGraph->valToNodeMap.end() &&
            depGraph->valToNodeMap.find(denominator) != depGraph->valToNodeMap.end()) {

          TaintDepGraphNode* numeratorNode = depGraph->valToNodeMap[numerator];
          TaintDepGraphNode* denominatorNode = depGraph->valToNodeMap[denominator];

          if (numeratorNode->isNodeTaintedInCurrentStack() &&
              denominatorNode->isNodeTaintedInCurrentStack()) {

            dprintf(1, "[NEW INFO] Found a nanSource with all parents tainted: ",
                    llvmToString(nanSource.first->val).c_str(), "\n");

            TaintDepGraphNode *nanSourceNode = nanSource.first;
            attackCtrlNAN->addNode(nanSourceNode, f);
          }
        }
      }
    }
  }

  depGraph->resetCurrentCallStack();
  funCS->pop();

  dprintf(1, "[STEP] Finished collapsing constraints for function: ",
          demangle(f->getName().str().c_str()).c_str(),
          "  Return val tainted ? ", isRetTainted ? "true" : "false", "\n");

  return isRetTainted;
}

void AchlysTaintChecker::insertFICall(Instruction *ii, Function *f, int id) {

  dprintf(1, "[STEP] Inserting FI call for instruction: ",
          llvmToString(ii).c_str(), "\n");

  // Insert the FI function
  IRBuilder<> IRB(ii->getParent());
  IRB.SetInsertPoint(ii->getNextNode());

  FunctionCallee Fn;

  // Currently, we support FI in only Float, Double, Int, and pointer types.
  if (ii->getType()->isPointerTy()) {
    Fn = ii->getFunction()->getParent()->getOrInsertFunction(
        "injectNANFaultPtr", ii->getType(), ii->getType(),
        Type::getInt32Ty(ii->getContext()));
  } else if (ii->getType()->isFloatTy()) {
    Fn = ii->getFunction()->getParent()->getOrInsertFunction(
        "injectNANFaultFloat", ii->getType(), ii->getType(),
        Type::getInt32Ty(ii->getContext()));
  } else if (ii->getType()->isDoubleTy()) {
    Fn = ii->getFunction()->getParent()->getOrInsertFunction(
        "injectNANFaultDouble", ii->getType(), ii->getType(),
        Type::getInt32Ty(ii->getContext()));
  } else if (ii->getType()->isIntegerTy()) {
    Fn = ii->getFunction()->getParent()->getOrInsertFunction(
        "injectNANFaultInt", ii->getType(), ii->getType(),
        Type::getInt32Ty(ii->getContext()));
  } else {
    dprintf(1, "[WARNING] Skipping FI call injection. Unsupported type: ",
            llvmToString(ii->getType()).c_str(), "\n");
    return;
  }

  // Create constant instruction.
  ConstantInt *constInt =
      ConstantInt::get(Type::getInt32Ty(ii->getContext()), id);
  Value *funret = IRB.CreateCall(Fn, {ii, constInt});

  // Replace all use of the arithmatic instruction with the function
  // return value
  auto myIf = [&](Use &operand) {
    if (isa<CallInst>(operand.getUser()))
      return false;
    return true;
  };

  ii->replaceUsesWithIf(funret, myIf);

  hasIRChanged = true;
}

// Add code for add instrumentations for FI.
void AchlysTaintChecker::doFaultInjectionInstrumentation(
    AttackerControlledNAN *nanSources) {

  for (auto item : nanSources->nodeToFunctionMap) {
    Function *f = item.second;
    TaintDepGraphNode *nanSourceNode = item.first;

    // Insert FI call right after the nanSource node.
    if (auto ii = dyn_cast<Instruction>(nanSourceNode->val))
      insertFICall(ii, f, nanSourceNode->attr.nanSourceNumber);
    else
      assert(false && "NanSource node is not an instruction");
  }
}

// Entry point of this pass.
bool AchlysTaintChecker::runOnModule(Module &M) {

  dprintf(1,
          "---------------Starting taint analysis pass-------------------\n");

  dprintf(1,
          addColor("*** Started Calculating function summaries ***\n", "yellow")
              .c_str());

  const clock_t begin_time = clock();

  // Iterate through all function in the program and find the main() function.
  for (Function &F : M) {

    if (isUserDefinedFunction(F) && F.getName().str().compare("main") == 0) {

      // main() function takes 2 arguments or none
      if (F.arg_size() == 0) {
        funcWorklist.push({&F, {nullptr, nullptr, {0}}});
      } else if (F.arg_size() == 2) {
        funcWorklist.push({&F, {nullptr, nullptr, {1, 2}}});
      } else {
        assert(false && "Assuming that main() function takes either 0 or \
                           2 arguments");
      }
      break;
    }
  }

  if (funcWorklist.size() == 0) {
    output << "Could not find main function! Aborting!\n";
    gracefulExit();
    return hasIRChanged;
  } else {

    while (funcWorklist.size() != 0) {
      auto elem = funcWorklist.front();
      Function *F = elem.first;
      FunctionContext fc = elem.second;

      funcWorklist.pop();

      if (analyzedFunctions.find(F) == analyzedFunctions.end()){

        analyzeFunction(F, &fc);
        analyzeFunction(F, &fc, true);
        analyzedFunctions.insert(F);
      }
    }
  }

  float timeTook = float(clock() - begin_time) / CLOCKS_PER_SEC;
  dprintf(1, "-----------Finished Calculating Function Summaries: time = ",
          to_string(timeTook).c_str(), " Seconds \n");

  dprintf(
      1,
      addColor("*** Started Collapsing constraints ***\n", "yellow").c_str());

  AttackerControlledNAN attackCtrlNAN;
  FunctionCallStack fcs;
  collapseConstraints(M.getFunction("main"), &fcs, {1, 2}, &attackCtrlNAN);

  dprintf(
      1, addColor("*** Started Filtering attacker controlled NaN sources ***\n",
                  "yellow")
             .c_str());

  // Check which attacker controlled NaN sources can potentially alter the
  // control flow of the application.
  filterAttackerControlledNANSources(&attackCtrlNAN);

  if (doFaultInjection) {
    dprintf(1, addColor("*** Started injecting Fault Injection related "
                        "instrumentation ***\n",
                        "yellow")
                   .c_str());

    doFaultInjectionInstrumentation(&attackCtrlNAN);
  } else {
    dprintf(
        1,
        addColor(
            "*** Fault Injection skipped. Turn it on using -doFaultInjection "
            "CLI flag. ***\n",
            "yellow")
            .c_str());
  }

  gracefulExit();
  return hasIRChanged;
}

// Our taint analysis pass depends on these LLVM passes.
void AchlysTaintChecker::getAnalysisUsage(AnalysisUsage &AU) const {
  AU.setPreservesCFG();
  AU.addRequired<AAResultsWrapperPass>();
  AU.addRequired<MemoryDependenceWrapperPass>();
  AU.addRequired<LoopInfoWrapperPass>();
}
} // namespace achlys

char achlys::AchlysTaintChecker::ID = 0;
static RegisterPass<achlys::AchlysTaintChecker>
    X("taintanalysis", "Pass to find tainted variables", false, true);

/* ********* TaintChecker.cpp *************
 *
 * Taint Analysis for Achlys.
 * Author(s): Udit Kumar Agarwal
 */

#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <cxxabi.h>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/IntrinsicInst.h"

using namespace llvm;
using namespace std;

#include "TaintChecker.h"

namespace {
struct AchlysTaintChecker : public FunctionPass {

  unordered_set<Value *> Tainted;
  unordered_map<Value *, int> OrderedTaintedVariableLines;
  map<int, Value *> OrderedTaintedVariables;
  map<int, string> Result;
  unordered_map<Value *, Instruction *> VarDeclMap;

  // Constructor
  static char ID;
  AchlysTaintChecker() : FunctionPass(ID) {}

  // If the variable declaration dominates the end, we consider it
  bool isInScope(Value *TaintedVar, BasicBlock *BB_End, DominatorTree &DT) {
    Instruction *VarDbgDecl = VarDeclMap[TaintedVar];
    BasicBlock *BB_DI = VarDbgDecl->getParent();
    return DT.dominates(BB_DI, BB_End);
  }

  bool processUntaintedValues(StoreInst *SI, PostDominatorTree &PDT) {
    if (auto TaintedVar = dyn_cast<Instruction>(&*(SI->getPointerOperand()))) {
      BasicBlock *BB_SI = SI->getParent();
      BasicBlock *BB_AI = TaintedVar->getParent();

      if (!PDT.dominates(BB_SI, BB_AI)) {
        return false;
      }

      int LineNumber = getSourceCodeLine(SI);
      if (LineNumber < OrderedTaintedVariableLines[TaintedVar]) {
        return false;
      }

      Tainted.erase(TaintedVar);
      string ResultString = "Line " + to_string(LineNumber) + ": " +
                            TaintedVar->getName().str() + " is now untainted";
      Result[LineNumber] = ResultString;
      int OriginLineNumber = OrderedTaintedVariableLines[TaintedVar];
      OrderedTaintedVariableLines.erase(TaintedVar);
      OrderedTaintedVariables.erase(OriginLineNumber);
      return false;
    }
    return true;
  }

  void logTaintedLine(Instruction *I, Value *TaintedVar) {
    int LineNumber = getSourceCodeLine(I);
    string ResultString = "Line " + to_string(LineNumber) + ": " +
                          TaintedVar->getName().str() + " is tainted";
    Result[LineNumber] = ResultString;
    OrderedTaintedVariableLines[TaintedVar] = LineNumber;
    OrderedTaintedVariables[LineNumber] = TaintedVar;
  }

  bool instrsAreInSameLoop(LoopInfo &LoopInfo, Instruction *Instr1,
                           Instruction *Instr2) {
    for (auto Loop : LoopInfo)
      if (instrIsInLoopUtil(Loop, Instr1) && instrIsInLoopUtil(Loop, Instr2))
        return true;
    return false;
  }

  bool instrIsInLoopUtil(Loop *Loop, Instruction *Instr) {
    if (Loop->contains(Instr))
      return true;

    return false;
  }

  // Function to find tainted variables
  bool runOnFunction(Function &F) override {
    std::string FuncName = demangle(F.getName().str().c_str());
    queue<Instruction *> Worklist;
    BasicBlock *BB_End = &(F.back());

    DominatorTree &DT = getAnalysis<DominatorTreeWrapperPass>().getDomTree();
    PostDominatorTree &PDT =
        getAnalysis<PostDominatorTreeWrapperPass>().getPostDomTree();

    LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

    // Only process main function
    if (FuncName != "main") {
      return false;
    }

    // Iterate through to check for sink sources.
    for (Function::const_iterator BI = F.begin(); BI != F.end(); ++BI) {
      // Iterate over all the instructions within a basic block.
      for (BasicBlock::const_iterator It = BI->begin(); It != BI->end(); ++It) {

        Instruction *Ins = const_cast<Instruction *>(&*It);

        if (DbgDeclareInst *Dbg = dyn_cast<DbgDeclareInst>(&*Ins)) {
          if (AllocaInst *DbgAI = dyn_cast<AllocaInst>(Dbg->getAddress())) {
            VarDeclMap[DbgAI] = Dbg;
          }
        } else if (auto CI = dyn_cast<CallInst>(&*Ins)) {
          StringRef CIFuncName = CI->getCalledFunction()->getName();
          string DemangledFuncCallName = demangle(CIFuncName.str().c_str());
          string DemangledFuncName =
              demangle(CI->getArgOperand(0)->getName().str().c_str());

          if (DemangledFuncCallName.find("istream") != string::npos &&
              DemangledFuncName.find("cin") != string::npos) {

            Value *TaintedVar = CI->getArgOperand(1);

            logTaintedLine(CI, TaintedVar);

            for (User *U : TaintedVar->users()) {
              Instruction *TaintedUserInst = dyn_cast<Instruction>(U);

              int LineNumber = getSourceCodeLine(TaintedUserInst);
              if (OrderedTaintedVariableLines[TaintedVar] > 0 &&
                  (LineNumber > OrderedTaintedVariableLines[TaintedVar] ||
                   instrsAreInSameLoop(LI, CI, TaintedUserInst))) {

                Worklist.push(TaintedUserInst);
              }
            } // End user loop
          }   // End istream condition
        }     // End DbgDeclareInst and CallInst conditions
      }       // End basic block loop
    }         // End function loop

    // Recursively traverse the def-use chain of suspected tainted values
    while (!Worklist.empty()) {
      Instruction *TaintedUser = Worklist.front();
      Worklist.pop();

      if (Tainted.count(TaintedUser) > 0) {
        continue;
      }

      if (auto SI = dyn_cast<StoreInst>(&*TaintedUser)) {
        // Recursively explore tainted instructions

        if (isa<Constant>(SI->getValueOperand()) &&
            !processUntaintedValues(SI, PDT)) {
          // Assigned to another constant
          continue;
        }

        if (auto TaintedVar =
                dyn_cast<Instruction>(&*(SI->getPointerOperand()))) {

          if (Tainted.count(TaintedVar) > 0) {
            continue;
          }

          Tainted.insert(TaintedVar);
          logTaintedLine(SI, TaintedVar);

          for (User *U : TaintedVar->users()) {
            Instruction *Ins = dyn_cast<Instruction>(U);

            int LineNumber = getSourceCodeLine(Ins);
            if (OrderedTaintedVariableLines[TaintedVar] > 0 &&
                (LineNumber > OrderedTaintedVariableLines[TaintedVar] ||
                 instrsAreInSameLoop(LI, TaintedVar, Ins))) {
              Worklist.push(Ins);
            }
          }
        }
      }

      for (User *U : TaintedUser->users()) {
        Instruction *Ins = dyn_cast<Instruction>(U);
        Worklist.push(Ins);
      }
    } // End Def-Use WorkList loop

    // Remove out of scope variables
    for (auto It = OrderedTaintedVariables.cbegin();
         It != OrderedTaintedVariables.cend();) {
      int TempLineNumber = It->first;
      Value *TempTaintedVar = It->second;
      bool InScope = isInScope(TempTaintedVar, BB_End, DT);
      if (!InScope) {
        Result.erase(TempLineNumber);
        OrderedTaintedVariableLines.erase(TempTaintedVar);
        It = OrderedTaintedVariables.erase(It);
      } else {
        ++It;
      }
    }

    // Print out tainted lines
    for (auto &line : Result) {
      errs() << line.second << "\n";
    }

    // Print set of tainted variables.
    string taintedVarSetStr = "Tainted: {";
    int countTainted = 0;
    for (auto const &i : OrderedTaintedVariables) {
      if (countTainted > 0) {
        taintedVarSetStr += ",";
      }
      taintedVarSetStr += i.second->getName().str();
      countTainted++;
    }
    taintedVarSetStr += "}";
    errs() << taintedVarSetStr << "\n";

    return false;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<PostDominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.setPreservesAll();
  }
};
} // namespace

char AchlysTaintChecker::ID = 0;
static RegisterPass<AchlysTaintChecker> X("taintanalysis",
                                   "Pass to find tainted variables");

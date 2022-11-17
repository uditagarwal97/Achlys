/* ********* TaintChecker.h *************
 *
 * Taint Analysis for Achlys.
 * Author(s): Udit Kumar Agarwal
*/
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"

#include <cxxabi.h>
#include <iostream>
#include <string>
#include <queue>
#include <vector>
#include <set>
#include <cassert>
#include <unordered_map>
#include <initializer_list>

// Output strings for debugging
std::string debug_str;
raw_string_ostream debug(debug_str);

// Strings for output
std::string output_str;
raw_string_ostream output(output_str);

namespace achlys {
//////////////////////////// Supporting Functions //////////////////////////////

// Printf with custom verbose levels.
void dprint(unsigned v, int numArgs, ...) {

  va_list vl;
  va_start(vl, numArgs);
  for (int i = 0; i < numArgs; i++) {
    if (Verbose == 4) {
      errs()<<va_arg(vl, char*);
    }
    else if (v <= Verbose) {
      debug<<va_arg(vl, char*);
    }
  }
  va_end(vl);
}

// Convert any LLVM op=bject to C string.
// Useful for console printing.
template<typename T>
string llvmToString(T *val) {

  std::string temp;
  raw_string_ostream temp_ostream(temp);

  temp_ostream << *val;

  string tempVar;
  tempVar.append(temp_ostream.str());
  return tempVar;
}

// Demangles the function name.
std::string demangle(const char *name) {
  int status = -1;

  std::unique_ptr<char, void (*)(void *)> res{
      abi::__cxa_demangle(name, NULL, NULL, &status), std::free};
  return (status == 0) ? res.get() : std::string(name);
}

// Returns the source code line number cooresponding to the LLVM instruction.
// Returns -1 if the instruction has no associated Metadata.
int getSourceCodeLine(Instruction *I) {
  // Get debugInfo associated with every instruction.
  llvm::DebugLoc debugInfo = I->getDebugLoc();

  int line = -1;
  if (debugInfo)
    line = debugInfo.getLine();

  return line;
}

/////////////////////////// Supporting Data Structures /////////////////////////

// Structure of dependency graph node.
// Dependency graph is a directed, 2-level graph, with the top nodes being
// either the base pointers or tainted function arguments. The bottom nodes are
// the tainted variables or pointers that depends on the top nodes.
struct TaintDepGraphNode {
  Value *val;
  enum NodeType {DEF_TAINT_SOURCE, POS_TAINT_SOURCE, POS_TAINT_VAR} type;

  // Debug attributes for this node.
  struct attributes {
    bool isReturnCallNode;
    bool isArgumentNode;
    bool isReturnValue;
    vector<Value*> callNodeArgs;

    attributes() {
      isReturnCallNode = false;
      isArgumentNode = false;
      isReturnValue = false;
    }
  } attr;

  // For storing child or parent.
  std::vector<TaintDepGraphNode *> children;

  TaintDepGraphNode(Value *v) : val(v) {}
};

// Structure of taint summary graph.
struct TaintDepGraph {
  std::vector<TaintDepGraphNode *> topLevelNodes;
  std::vector<TaintDepGraphNode *> bottomLevelNodes;
  std::unordered_map<Value*, TaintDepGraphNode *> valToNodeMap;

  Function *F;

  TaintDepGraph(Function *_F) : F{_F} {}

  void addTopLevelNode(TaintDepGraphNode *node) {
    topLevelNodes.push_back(node);
  }

  void addBottomLevelNode(TaintDepGraphNode *node) {
    bottomLevelNodes.push_back(node);
  }

  void addFunctionArgument(Value* v) {
    TaintDepGraphNode *node = new TaintDepGraphNode(v);
    node->attr.isArgumentNode = true;
    node->type = TaintDepGraphNode::NodeType::POS_TAINT_SOURCE;
    valToNodeMap.insert({v, node});

    addTopLevelNode(node);
  }

  // Add valToBeTainted to the graph, if atleast one of the value in dependsVals
  // is tainted.
  void checkAndPropogateTaint(Value* valToBeTainted, vector<Value*> dependVals){

    bool isTainted = false;
    vector<Value*> taintDepSet;

    for (Value* v : dependVals) {
      if (valToNodeMap.find(v) != valToNodeMap.end()) {
        isTainted = true;
        taintDepSet.push_back(v);
      }
    }

    // Add the new node to the graph.
    if (isTainted) {
      TaintDepGraphNode *node = new TaintDepGraphNode(valToBeTainted);
      node->type = TaintDepGraphNode::NodeType::POS_TAINT_VAR;
      valToNodeMap.insert({valToBeTainted, node});

      addBottomLevelNode(node);

      for (Value* v : taintDepSet) {
        TaintDepGraphNode *vNode = valToNodeMap[v];

        // If it is a top-level node.
        if (vNode->type == TaintDepGraphNode::NodeType::POS_TAINT_SOURCE ||
            vNode->type == TaintDepGraphNode::NodeType::DEF_TAINT_SOURCE) {
          vNode->children.push_back(node);
          node->children.push_back(vNode);
        }
        else {
          // If it is a bottom-level node.
          for (TaintDepGraphNode *parent : vNode->children) {
            parent->children.push_back(node);
            node->children.push_back(parent);
          }
        }
      }
    }
  }

  // remove v from the graph.
  void removeTaint(Value* v) {

    if (valToNodeMap.find(v) != valToNodeMap.end()) {
      TaintDepGraphNode *node = valToNodeMap[v];

      // Remove the node from the bottom level nodes.
      for (auto it = bottomLevelNodes.begin(); it != bottomLevelNodes.end();
           it++) {
        if (*it == node) {
          bottomLevelNodes.erase(it);
          break;
        }
      }

      // Remove the node from the top level nodes.
      for (auto it = topLevelNodes.begin(); it != topLevelNodes.end(); it++) {
        if (*it == node) {
          topLevelNodes.erase(it);
          break;
        }
      }

      // Remove the node from the children of its parents.
      for (TaintDepGraphNode *parent : node->children) {
        for (auto it = parent->children.begin(); it != parent->children.end();
             it++) {
          if (*it == node) {
            parent->children.erase(it);
            break;
          }
        }
      }

      // Remove the node from the valToNodeMap.
      valToNodeMap.erase(v);
      delete node;
    }
  }

  void addCallSiteReturnTaint(Value* retVal, Function* callee,
                              vector<Value*> taintedArgs) {

    // If the return value is tainted.
    TaintDepGraphNode* node = new TaintDepGraphNode(retVal);
    node->type = TaintDepGraphNode::NodeType::POS_TAINT_SOURCE;
    node->attr.isReturnCallNode = true;
    node->attr.callNodeArgs = taintedArgs;

    valToNodeMap.insert({retVal, node});
    topLevelNodes.push_back(node);
  }

  void addTaintSource(Value* v) {
    TaintDepGraphNode *node = new TaintDepGraphNode(v);
    node->type = TaintDepGraphNode::NodeType::DEF_TAINT_SOURCE;
    valToNodeMap.insert({v, node});

    addTopLevelNode(node);
  }

  void markReturnValue(Value* v) {
    if (valToNodeMap.find(v) != valToNodeMap.end()) {
      TaintDepGraphNode *node = valToNodeMap[v];
      node->attr.isReturnValue = true;
    }
  }

  void printGraph(int logLevel) {
    dprintf(logLevel, "Printing summary graph for function ",
            F->getName().str().c_str(), " \n\n");

    for (auto node : topLevelNodes) {
      dprintf(logLevel, "\033[0;33m Top level node: \033[0m",
              llvmToString(node->val).c_str());

      if (node->attr.isArgumentNode) {
        dprintf(logLevel, " \033[0;31m (Argument node) \033[0m");
      }
      else if (node->attr.isReturnCallNode) {
        dprintf(logLevel, " \033[0;31m (Call Site node) \033[0m");
      }
      else if (node->attr.isReturnValue) {
        dprintf(logLevel, " \033[0;31m (Return value) \033[0m");
      }

      dprintf(logLevel, "\n");
      dprintf(logLevel, "------------------------------------\n");

      for (auto child : node->children) {
        dprintf(logLevel, "\033[0;33m Child: \033[0m",
                llvmToString(child->val).c_str());

        if (child->attr.isReturnValue) {
          dprintf(logLevel, " \033[0;31m (Return value) \033[0m");
        }

        dprintf(logLevel, "\n");
      }

      dprintf(logLevel, "------------------------------------\n");
    }
  }
};

// This FunctionContext is stored with each function in the FunctionWorklist.
// For the main() function, all parameters are considered tainted.
struct FunctionContext {

  // For main() function callInst and caller will be NULL; Please check for that.
  Instruction *callInst;
  Function *caller;

  // Which arguments of this funtion are tainted?
  vector<int> numArgTainted;

  FunctionContext (Instruction* ii, Function *c, vector<int> numArg) {
    callInst = ii;
    caller = c;
    numArgTainted.insert(numArgTainted.begin(), numArg.begin(), numArg.end());

    assert(numArgTainted.size() > 0 && "Atleast one argument should be tainted.\
                                         Or pass 0 for none.");

    std::sort(numArgTainted.begin(), numArgTainted.end());
  }
};

// Every function has this DS which tracks all the Taint-related information.
struct FunctionTaintSet {

  // All variables in this set are (or might be) tainted!
  // Variabes can have 4 states {UNKNOWN, TAINTED, UNTAINTED, DEPENDS}
  // The DEPENDS state means that the taintedness of this variables depends
  // on the return value of a call instruction.
  unordered_map<Value*, vector<Value*>> taintSet;

  // Where in this function can NaN be generated?
  // Tracks the first instruction that can generate a NaN.
  set<Value*> nanSources;

  // Values that are both tainted and NaNs.
  set<Value*> taintedNans;

  pair<bool, vector<Value*>> isReturnValueTainted;

  // Has the FunctionTaintSet changed!
  bool hasChanged;

  // Mark the 'val' as return value of this function.
  void markThisValueAsReturnValue(Value* val) {

    if (taintSet.find(val) != taintSet.end()) {
      isReturnValueTainted.first = true;
      isReturnValueTainted.second = taintSet[val];
    }
    else {
      isReturnValueTainted.first = false;
    }
  }

  // Check is any value from dependVals is tainted, if so, add valueToBeTainted
  // to the taint set.
  void checkAndPropagateTaint(Value* valueToBeTainted,
                              vector<Value*> dependVals = {}) {

    vector<Value*> depends;
    bool isUnConditionalTaint = false;
    bool isNaN = false;

    // If there is no taint dependency, then always taint this value.
    // It's usefull for user inputs or taint sources.
    bool isTaint = (dependVals.size() == 0) ? true : false;

    for (Value* val : dependVals) {

      if (taintSet.find(val) != taintSet.end()) {
        depends.insert(depends.begin(), taintSet[val].begin(),
                      taintSet[val].end());
        isTaint = true;

        if (taintSet[val].size() == 0) isUnConditionalTaint = true;

        if (taintedNans.find(val) != taintedNans.end()) {
          isNaN = true;
        }
      }
    }

    if (isNaN) {
      taintedNans.insert(valueToBeTainted);
    }

    if (isUnConditionalTaint) depends = {};

    if (isTaint) {
      taintSet.insert({valueToBeTainted, depends});
      hasChanged = true;
    }
  }

  void taintFunctionReturnValue(Value* valToTaint, Value* callInst) {

    taintSet.insert({valToTaint, {callInst}});
  }

  void addNaNSource(Value* val) {
    nanSources.insert(val);
    taintedNans.insert(val);
  }

  bool isTainted(Value* val) {
    return taintSet.find(val) != taintSet.end();
  }

  bool isUnconditionalTainted(Value * val) {

    return (taintSet.find(val) != taintSet.end() &&
            taintSet[val].size() == 0);
  }

  bool isNanValue(Value* val) {
    return taintedNans.find(val) != taintedNans.end();
  }

  void removeTaint(Value* val) {
    taintSet.erase(val);

    if (nanSources.find(val) != nanSources.end())
      nanSources.erase(val);

    if (taintedNans.find(val) != taintedNans.end())
      taintedNans.erase(val);
  }

  void snapshot() {
    hasChanged = false;
  }

  void summarize(int logLevel) {
    dprintf(logLevel,
              "\033[0;32m----------Summarizing Taint Set------------------\n");

    for (auto pp : taintSet) {
      dprintf(logLevel, llvmToString(pp.first).c_str(), " : depends on = {");
      for(auto di : pp.second)
        if (di)
          dprintf(logLevel, llvmToString(di).c_str(),", ");
      dprintf(logLevel, "}\n");
    }

    dprintf(logLevel, "----------End Summarizing Taint Set--------------\033[0m\n");

    dprintf(logLevel, "\033[0;32m----------Summarizing NaN Set------------------\n");

    for (auto pp : taintedNans) {
      if (pp)
        dprintf(logLevel, llvmToString(pp).c_str(), "\n");
    }

    dprintf(logLevel, "----------End Summarizing NaN Set--------------\033[0m\n");
  }

  FunctionTaintSet() {
    hasChanged = false;
  }
};

// LLVM pass declaration for taint analysis.
struct AchlysTaintChecker : public ModulePass {

  // Worklist for functions.
  queue<pair<Function*, FunctionContext>> funcWorklist;

  // Mapping between a function and its FunctionTaintSet.
  unordered_map<Function*, FunctionTaintSet> funcTaintSet;

  // Mapping between a function and its Taint Summary Graph.
  unordered_map<Function*, TaintDepGraph*> funcTaintSummaryGraph;

  // Cache results for inter-procedural alias analysis.
  AAResults *aliasAnalysisResult;

  // Constructor
  static char ID;
  AchlysTaintChecker() : ModulePass(ID) {}

  // Check if a binary instruction is constant or not.
  // a - a, a xor a, a X 0, a / a are all constant instructions.
  // We don't propogate taint info. upon finding a constant instruction.
  bool isConstantInstruction(int opcode, Value* operand1, Value* operand2) {

    if (opcode == Instruction::Sub || opcode == Instruction::FSub ||
        opcode == Instruction::Xor || opcode == Instruction::FDiv ||
        opcode == Instruction::SDiv) {

          // If the operands are equal or aliases.
          if (operand1 == operand2 || aliasAnalysisResult->isMustAlias(operand1,
              operand2)) {
            return true;
          }
    }

    // Check for multiply by zero.
    if (opcode == Instruction::FMul || opcode == Instruction::Mul) {
      if (auto ii = dyn_cast<Constant>(operand1)) {
        if (ii->isZeroValue()) return true;
      }
      else if (auto i = dyn_cast<Constant>(operand2)) {
        if (i->isZeroValue()) return true;
      }
    }

    return false;
  }

  // Check if this function is user-defined or system-defined.
  bool isUserDefinedFunction(Function& F) {
    // If this   is not a user-defined function.
    if (F.isDeclaration() || F.getName().str().find("std") != string::npos)
      return false;

    return true;
  }

  // [Taint Sources] Check if this function can act as taint sources.
  // Functions like file.read(), istream.*, read(), aio_read(), fread() syscall.
  bool isTaintSourceFunction(Function &F) {

    string funcName = demangle(F.getName().str().c_str());

    if (funcName == "fread" || funcName.find("istream") != string::npos ||
        funcName == "read" || funcName == "aio_read")
        return true;

    return false;
  }

  // Check if this function can produce NaNs in the code.
  // Functions like atof(), strtod(), strtof() can produce NaN is the input
  // string is tainted.
  bool isNaNSourceFunction(Function &F) {

    string funcName = F.getName().str();

    if (funcName == "atof" || funcName == "strtod" || funcName == "strtof")
      return true;

    return false;
  }

  // Single exit point for our analysis pass.
  // Before exiting, print debug and output info on console.
  void gracefulExit() {
    #ifdef __DEBUG__
      dprintf(1,
              "---------------Ending taint analysis pass-------------------\n");
      errs() << debug.str();
    #endif
    debug.flush();

    errs() << output.str();
    output.flush();
  }

  // Analyze each instruction one by one. Essentially, this function will apply
  // taint propogation and eviction policies on each instruction.
  void analyzeInstruction(Instruction*, FunctionTaintSet*,
                          FunctionContext, TaintDepGraph*);

  // Analyze each function one by one. We will use a lattice-based fixpoint
  // iterative, inter-procedural data flow analysis.
  // TODO:
  //    - Handle recursive function calls.
  //    - Handle pointers and data structures (Abstract memory model)
  void analyzeFunction(Function *F, FunctionContext fc);

  // Entry point of this pass.
  bool runOnModule(Module& M) override;

  // Our taint analysis pass depends on these LLVM passes.
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};
} // End of namespace.

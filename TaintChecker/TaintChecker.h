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
#include <stack>
#include <unordered_map>
#include <unordered_set>
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

// Color string for console printing.
std::string addColor(string s, string color) {

  string color_str;

  if (color == "yellow") {
    color_str = "\033[1;33m";
  }
  else if (color == "red") {
    color_str = "\033[1;31m";
  }
  else if (color == "green") {
    color_str = "\033[1;32m";
  }
  else if (color == "blue") {
    color_str = "\033[1;34m";
  }
  else if (color == "magenta") {
    color_str = "\033[1;35m";
  }
  else if (color == "cyan") {
    color_str = "\033[1;36m";
  }
  else if (color == "white") {
    color_str = "\033[1;37m";
  }

  color_str = color_str + s + "\033[0m";
  return color_str;
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

// Give a unique ID to each NaN source.
int nanSourceCount = 0;

// Structure of dependency graph node.
// Dependency graph is a directed, 2-level graph, with the top nodes being
// either the base pointers or tainted function arguments. The bottom nodes are
// the tainted variables or pointers that depends on the top nodes.
struct TaintDepGraphNode {
  Value *val;
  enum NodeType {UNKNOWN, DEF_TAINT_SOURCE, POS_TAINT_SOURCE, POS_TAINT_VAR}
                type;
  enum NodeNaNStatus {NAN_UNKNOWN, NAN_SOURCE, TAINTED_NAN} nanStatus;

  // Debug attributes for this node.
  struct attributes {
    bool isReturnCallNode;
    bool isArgumentNode;
    bool isReturnValue;
    int argumentNumber;
    int nanSourceNumber;
    vector<Value*> callNodeArgs;
    set<int> derivedNaNSourceId;

    attributes() {
      isReturnCallNode = false;
      isArgumentNode = false;
      isReturnValue = false;
      argumentNumber = -1;
      nanSourceNumber = -1;
    }
  } attr;

  // Is this node tainted in current function call stack?
  bool isTaintedInCurrentStack;

  // For storing child or parent.
  std::vector<TaintDepGraphNode *> children;

  void addChildOrParent(TaintDepGraphNode *node) {

    // Check if node is already present in children.
    if (find(children.begin(), children.end(), node) == children.end())
      children.push_back(node);
  }

  bool isNaNSource() {
    return (nanStatus == NAN_SOURCE);
  }

  // These functions are used to store taint information during inter-procedural,
  // context-sensitive analysis.
  void setCurrentStackTaint() {
    isTaintedInCurrentStack = true;
  }

  bool isNodeTaintedInCurrentStack() {
    return isTaintedInCurrentStack;
  }

  void resetCurrentCallStack() {
    isTaintedInCurrentStack = false;
  }

  TaintDepGraphNode(Value *v) : val(v), type(UNKNOWN), nanStatus(NAN_UNKNOWN),
                                isTaintedInCurrentStack(false) {}
};

// Structure of taint summary graph.
struct TaintDepGraph {
  std::vector<TaintDepGraphNode *> topLevelNodes;
  std::unordered_map<Value*, TaintDepGraphNode *> valToNodeMap;
  std::vector<TaintDepGraphNode *> callSiteReturnNode;

  Function *F;

  TaintDepGraph(Function *_F) : F{_F} {}

  void addTopLevelNode(TaintDepGraphNode *node) {
    topLevelNodes.push_back(node);
  }

  void addFunctionArgument(Value* v, int argNum) {

    // Check if the function argument is constant or not.
    if(isa<Constant>(v))
      return;

    TaintDepGraphNode *node = new TaintDepGraphNode(v);
    node->attr.isArgumentNode = true;
    node->type = TaintDepGraphNode::NodeType::POS_TAINT_SOURCE;
    node->attr.argumentNumber = argNum;
    valToNodeMap.insert({v, node});

    if (v->getType()->isFloatTy() || v->getType()->isDoubleTy()) {
      node->nanStatus = TaintDepGraphNode::NodeNaNStatus::NAN_SOURCE;
      node->attr.nanSourceNumber = ++nanSourceCount;
    }

    addTopLevelNode(node);
  }

  bool isTainted(Value* v) {
    if(valToNodeMap.find(v) != valToNodeMap.end())
      return true;

    return false;
  }

  bool isTopLevelNode(TaintDepGraphNode *node) {
    for (auto n : topLevelNodes) {
      if (n == node)
        return true;
    }
    return false;
  }

  void markValueAsNaNSource(Value* v, bool isDefTaintSource = false) {
    if(valToNodeMap.find(v) != valToNodeMap.end()) {
      valToNodeMap[v]->nanStatus = TaintDepGraphNode::NodeNaNStatus::NAN_SOURCE;
      valToNodeMap[v]->attr.nanSourceNumber = ++nanSourceCount;

      if (isDefTaintSource)
        valToNodeMap[v]->type = TaintDepGraphNode::NodeType::DEF_TAINT_SOURCE;
    }
  }

  // Add valToBeTainted to the graph, if atleast one of the value in dependsVals
  // is tainted.
  void checkAndPropogateTaint(Value* valToBeTainted, vector<Value*> dependVals){

    // check if valToBeTainted is already tainted.
    if (valToNodeMap.find(valToBeTainted) != valToNodeMap.end())
      return;

    bool isTainted = false;
    bool isAnyDepNaN = false;
    vector<Value*> taintDepSet;
    set<int> nanSourceDepSet;

    for (Value* v : dependVals) {
      if (valToNodeMap.find(v) != valToNodeMap.end()) {
        isTainted = true;

        // Check if any dependency is NaN.
        if (valToNodeMap[v]->nanStatus ==
            TaintDepGraphNode::NodeNaNStatus::NAN_SOURCE) {

          nanSourceDepSet.insert(valToNodeMap[v]->attr.nanSourceNumber);
          isAnyDepNaN = true;
        }
        else if (valToNodeMap[v]->nanStatus ==
            TaintDepGraphNode::NodeNaNStatus::TAINTED_NAN) {

          isAnyDepNaN = true;
          nanSourceDepSet.insert(valToNodeMap[v]->attr.derivedNaNSourceId.begin(),
                                 valToNodeMap[v]->attr.derivedNaNSourceId.end());
        }

        taintDepSet.push_back(v);
      }
    }

    // Add the new node to the graph.
    if (isTainted) {
      TaintDepGraphNode *node = new TaintDepGraphNode(valToBeTainted);
      node->type = TaintDepGraphNode::NodeType::POS_TAINT_VAR;
      valToNodeMap.insert({valToBeTainted, node});

      if (isAnyDepNaN) {
        node->nanStatus = TaintDepGraphNode::NodeNaNStatus::TAINTED_NAN;
        node->attr.derivedNaNSourceId.insert(nanSourceDepSet.begin(),
                                            nanSourceDepSet.end());
      }

      for (Value* v : taintDepSet) {
        TaintDepGraphNode *vNode = valToNodeMap[v];
        // Check the parent to which this node is already connected.
        unordered_set<TaintDepGraphNode*> parentConnected;

        // If it is a top-level node.
        if (vNode->type == TaintDepGraphNode::NodeType::POS_TAINT_SOURCE ||
            vNode->type == TaintDepGraphNode::NodeType::DEF_TAINT_SOURCE) {

          // Check if this node is already connected to the parent.
          if (parentConnected.find(vNode) == parentConnected.end()) {
            vNode->addChildOrParent(node);
            node->addChildOrParent(vNode);
            parentConnected.insert(vNode);
          }
        }
        else {
          // If it is a bottom-level node.
          for (TaintDepGraphNode *parent : vNode->children) {

            // Check if this node is already connected to the parent.
            if (parentConnected.find(parent) == parentConnected.end()) {
              parent->addChildOrParent(node);
              node->addChildOrParent(parent);
              parentConnected.insert(parent);
            }
          }
        }
      }
    }
  }

  // remove v from the graph.
  void removeTaint(Value* v) {

    if (valToNodeMap.find(v) != valToNodeMap.end()) {
      TaintDepGraphNode *node = valToNodeMap[v];

      // Remove the node from the top level nodes.
      for (auto it = topLevelNodes.begin(); it != topLevelNodes.end(); it++) {
        if (*it == node) {
          topLevelNodes.erase(it);
          break;
        }
      }

      // Remove the node from the call site return node.
      for (auto it = callSiteReturnNode.begin(); it != callSiteReturnNode.end();
           it++) {
        if (*it == node) {
          callSiteReturnNode.erase(it);
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

  // Reset taint status of all the nodes in the graph.
  // This function is used for re-using function summaries across different
  // context.
  void resetCurrentCallStack() {
    for (auto it = valToNodeMap.begin(); it != valToNodeMap.end(); it++) {
      it->second->resetCurrentCallStack();
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
    callSiteReturnNode.push_back(node);
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
        dprintf(logLevel, " \033[0;31m (Argument node) Argument Index=",
                to_string(node->attr.argumentNumber).c_str()," \033[0m");
      }
      else if (node->attr.isReturnCallNode) {
        dprintf(logLevel, " \033[0;31m (Call Site node) \033[0m");
      }
      else if (node->attr.isReturnValue) {
        dprintf(logLevel, " \033[0;31m (Return value) \033[0m");
      }
      else if (node->nanStatus == TaintDepGraphNode::NodeNaNStatus::NAN_SOURCE) {
        dprintf(logLevel, " \033[0;31m (NaN source) Id=",
                  to_string(node->attr.nanSourceNumber).c_str(), " \033[0m");
      }
      else if (node->nanStatus ==
               TaintDepGraphNode::NodeNaNStatus::TAINTED_NAN) {
        dprintf(logLevel, " \033[0;31m (Tainted NaN) \033[0m");
      }

      dprintf(logLevel, "\n");
      dprintf(logLevel, "------------------------------------\n");

      for (auto child : node->children) {
        dprintf(logLevel, "\033[0;33m Child: \033[0m",
                llvmToString(child->val).c_str());

        if (child->attr.isReturnValue) {
          dprintf(logLevel, " \033[0;31m (Return value) \033[0m");
        }
        else if (child->nanStatus ==
                TaintDepGraphNode::NodeNaNStatus::NAN_SOURCE) {
          dprintf(logLevel, " \033[0;31m (NaN source) Id=",
                  to_string(child->attr.nanSourceNumber).c_str(), " \033[0m");
        }
        else if (child->nanStatus ==
                TaintDepGraphNode::NodeNaNStatus::TAINTED_NAN) {
          dprintf(logLevel, " \033[0;31m (Tainted NaN) \033[0m");
        }

        dprintf(logLevel, "\n");
      }

      dprintf(logLevel, "------------------------------------\n");
    }
  }
};

// DS to implement a function call stack for collapsing the constraints.
struct FunctionCallStack {
  stack<Function*> callStack;
  int length;
  unordered_set<Function*> functionsInCallStack;

  FunctionCallStack() {
    length = 0;
  }

  void push(Function* F) {
    callStack.push(F);
    length++;
    functionsInCallStack.insert(F);
  }

  void pop() {
    functionsInCallStack.erase(functionsInCallStack.find(callStack.top()));
    callStack.pop();
    length--;
  }

  bool isRecursion(Function *f) {
    if (functionsInCallStack.find(f) != functionsInCallStack.end()) {
      return true;
    }

    return false;
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

// DS to hold attacker controlled NAN sources
struct AttackerControlledNAN {
  unordered_map<TaintDepGraphNode*, Function*> nodeToFunctionMap;

  AttackerControlledNAN() {}

  void addNode(TaintDepGraphNode* node, Function* F) {
    nodeToFunctionMap.insert({node, F});
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

  // If IR has changed.
  bool hasIRChanged;

  // Constructor
  static char ID;
  AchlysTaintChecker() : ModulePass(ID), hasIRChanged(false) {}

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

  // Function to interprocedurally collapse constraints.
  bool collapseConstraints(Function*, FunctionCallStack*, vector<int>,
                          AttackerControlledNAN*);

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

  // Filter attacker controlled NaN nodes to keep only those that affects the
  // control-flow of the program.
  void filterAttackerControlledNANSources(
                              AttackerControlledNAN*);


  // Function to edit LLVM IR and add injectFault() calls.
  void insertFICall(Instruction*, Function*, int);

  // Add instrumentations for fault injection.
  void doFaultInjectionInstrumentation(AttackerControlledNAN*);

  // Entry point of this pass.
  bool runOnModule(Module& M) override;

  // Our taint analysis pass depends on these LLVM passes.
  void getAnalysisUsage(AnalysisUsage &AU) const override;
};
} // End of namespace.

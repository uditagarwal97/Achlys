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
#include <cassert>
#include <unordered_map>

// Output strings for debugging
std::string debug_str;
raw_string_ostream debug(debug_str);

// Strings for output
std::string output_str;
raw_string_ostream output(output_str);

namespace achlys {

/////////////////////////// Supporting Data Structures /////////////////////////

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
  }
};

// LLVM pass declaration for taint analysis.
struct AchlysTaintChecker : public ModulePass {

  // Worklist for functions.
  queue<pair<Function*, FunctionContext>> funcWorklist;

  // Constructor
  static char ID;
  AchlysTaintChecker() : ModulePass(ID) {}

  // Check if this function is user-defined or system-defined.
  bool isUserDefinedFunction(Function& F) {
    // If this   is not a user-defined function.
    if (F.isDeclaration() || F.getName().str().find("std") != string::npos)
      return false;

    return true;
  }

  // Single exit point for our analysis pass.
  // Before exiting, print debug and output info on console.
  void gracefulExit() {
    #ifdef __DEBUG__
      debug<<"---------------Ending taint analysis pass-------------------\n";
      errs() << debug.str();
    #endif
    debug.flush();

    errs() << output.str();
    output.flush();
  }

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
//////////////////////////// Supporting Functions //////////////////////////////

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
} // End of namespace.
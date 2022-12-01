/* ********* Tools.h *************
 *
 * Tool functions for Achlys.
 *
 */
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"

#include <cassert>
#include <cxxabi.h>
#include <initializer_list>
#include <iostream>
#include <queue>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Output strings for debugging
std::string debug_str;
raw_string_ostream debug(debug_str);

// Strings for output
std::string output_str;
raw_string_ostream output(output_str);

//////////////////////////// Supporting Functions //////////////////////////////

// Printf with custom verbose levels.
void dprint(unsigned v, int numArgs, ...) {

  va_list vl;
  va_start(vl, numArgs);
  for (int i = 0; i < numArgs; i++) {
    if (Verbose == 4) {
      errs() << va_arg(vl, char *);
    } else if (v <= Verbose) {
      debug << va_arg(vl, char *);
    }
  }
  va_end(vl);
}

// Convert any LLVM op=bject to C string.
// Useful for console printing.
template <typename T> string llvmToString(T *val) {

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
  } else if (color == "red") {
    color_str = "\033[1;31m";
  } else if (color == "green") {
    color_str = "\033[1;32m";
  } else if (color == "blue") {
    color_str = "\033[1;34m";
  } else if (color == "magenta") {
    color_str = "\033[1;35m";
  } else if (color == "cyan") {
    color_str = "\033[1;36m";
  } else if (color == "white") {
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

bool isAllocInst(auto inst) {
  if (auto tempInst = dyn_cast<AllocaInst>(inst)) {
    return true;
  }
  return false;
}

bool isValidPtrType(auto inst) {
  if (auto tempInst = dyn_cast<AllocaInst>(inst)) {
    if ((tempInst->getAllocatedType()->isPointerTy() ||
         tempInst->getAllocatedType()->isArrayTy() ||
         tempInst->getAllocatedType()->isStructTy())) {
      return true;
    }
  } else {
    if ((inst->getType()->isPointerTy() || inst->getType()->isArrayTy() ||
         inst->getType()->isStructTy())) {
      return true;
    }
  }
  return false;
}
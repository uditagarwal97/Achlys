/* ********* DataStruct.h *************
 *
 * Data structure to support Achlys.
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

#include "tools.h"

/////////////////////////// Supporting Data Structures /////////////////////////

// This data structure is used to store the pointer tree
// Each node has a list of pointers that derived from the same base pointer
// Each node also point to its parent
struct PtrDepTreeNode {
  Value *val;

  PtrDepTreeNode *parent;
  std::vector<PtrDepTreeNode *> children;

  bool isTaintedInCurrentStack;

  // Constructor
  PtrDepTreeNode(Value *v)
      : val(v), isTaintedInCurrentStack(false), parent(NULL) {}

  void addParent(PtrDepTreeNode *node) { parent = node; }

  void addChild(PtrDepTreeNode *node) { children.push_back(node); }

  void printPtrNode() {
    dprintf(1, "--> The current inst is : ", llvmToString(val).c_str(), "\n");
  }
};

// This data structure is used to store the tree like structure of derived
// pointer and base pointers It only has two levels The first level stores the
// base pointer from alloc inst The second levels are the lowest level derived
// pointers All intermiediate pointers are not stores We use the map to
// tranverse to construct this tree at the end of the program
struct PtrDepTree {
  std::vector<PtrDepTreeNode *> top_base_pointers;

  // Constructor
  PtrDepTree() {}

  void addToTop(PtrDepTreeNode *node) { top_base_pointers.push_back(node); }

  void printTopBasePtrList() {
    dprintf(1, "^^^^^ I am printing out the pointer tree\n");
    dprintf(1, "*********** start print current top level base pointers "
               "*************\n");
    for (int i = 0; i < top_base_pointers.size(); i++) {
      top_base_pointers[i]->printPtrNode();
    }
    dprintf(1, "^^^^^ top node size: ",
            to_string(top_base_pointers.size()).c_str(), "\n");
    dprintf(1, "*********** end print current top level base pointers "
               "*************\n");
  }
};

// This is a map to store the derived pointers and base(intermiediate) pointers
// This is used for fast access to store the information
// We will use it at the end of program to construct the pointer tree
struct PtrMap {
  // Mapping between a derived pointer and its base pointer.
  unordered_map<Value *, vector<Value *>> pointerSet;

  // Constructor
  PtrMap() {}

  // insert a new pair
  void insert(Value *key, Value *val) {
    if (key == NULL) {
      return;
    }
    if (pointerSet.find(key) != pointerSet.end()) {
      // this key has already in the map
      pointerSet.find(key)->second.push_back(val);
    } else {
      vector<Value *> val_list;
      if (val != NULL) {
        val_list.push_back(val);
      }
      pointerSet.insert({key, val_list});
    }
  }

  // tranverse the map to construct the tree
  Value *findBase(Value *derived_key) {
    Value *current = derived_key;
    // Value* val_list = pointerSet.find(key)->second;
    while (!isBasePointer(current)) {
      // TODO: need to find if one of the val in the val_list is a base
    }
  }

  // get map size
  int getSize() { return pointerSet.size(); }

  // print value vector
  void printValVector(vector<Value *> list) {
    for (int i = 0; i < list.size(); i++) {
      dprintf(1, " /// ", llvmToString(list[i]).c_str());
    }
    dprintf(1, " \n");
  }

  // check if a pointer in pointer map is a base pointer
  bool isBasePointer(Value *key) {
    if (pointerSet.find(key) == pointerSet.end()) {
      return false;
    }
    return pointerSet.find(key)->second.empty();
  }

  // print the map
  void printMap() {
    dprintf(1, "^^^^^ I am printing out the pointer map\n");
    dprintf(1, "*********** start print pointer map "
               "*************\n");
    for (std::pair<Value *, vector<Value *>> element : pointerSet) {
      if (element.first != NULL) {
        dprintf(1, "--> key: ", llvmToString(element.first).c_str(), "\n");
      } else {
        dprintf(1, "--> key is NULL\n");
      }
      if (!element.second.empty()) {
        dprintf(1, "--> value:");
        printValVector(element.second);
      } else {
        dprintf(1, "--> value is NULL\n");
      }
    }
    dprintf(1, "^^^^^ map size: ", to_string(pointerSet.size()).c_str(), "\n");
    dprintf(1, "*********** end print pointer map "
               "*************\n");
  }
};

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

  std::vector<PtrDepTreeNode *> parent;
  std::vector<PtrDepTreeNode *> children;

  bool isTaintedInCurrentStack;

  // Constructor
  PtrDepTreeNode(Value *v) : val(v), isTaintedInCurrentStack(false) {}

  void addParent(PtrDepTreeNode *node) { parent.push_back(node); }

  void addChild(PtrDepTreeNode *node) { children.push_back(node); }

  void printPtrNode() { dprintf(1, llvmToString(val).c_str(), "\n"); }
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

  bool isRoot(Value *val) {
    for (int i = 0; i < top_base_pointers.size(); i++) {
      if (top_base_pointers[i]->val == val) {
        return true;
      }
    }
    return false;
  }

  void addToTop(PtrDepTreeNode *node) { top_base_pointers.push_back(node); }

  void printTopBasePtrList() {
    dprintf(1, "^^^^^ I am printing out the pointer tree\n");
    dprintf(1, "*********** start print current top level base pointers "
               "*************\n");
    for (int i = 0; i < top_base_pointers.size(); i++) {
      dprintf(1, "--> root: ");
      top_base_pointers[i]->printPtrNode();
      for (int j = 0; j < top_base_pointers[i]->children.size(); j++) {
        dprintf(1, "--> child: ");
        top_base_pointers[i]->children[j]->printPtrNode();
      }
    }
    dprintf(1, "^^^^^ top node size: ",
            to_string(top_base_pointers.size()).c_str(), "\n");
    dprintf(1, "*********** end print current top level base pointers "
               "*************\n");
  }
  void printSecondLevelPtrList() {
    dprintf(1, "^^^^^ I am printing out the second level of the tree\n");
    dprintf(1, "*********** start print the second level of the tree "
               "*************\n");
    int size = 0;
    for (int i = 0; i < top_base_pointers.size(); i++) {
      for (int j = 0; j < top_base_pointers[i]->children.size(); j++) {
        dprintf(1, "--> child: ");
        top_base_pointers[i]->children[j]->printPtrNode();
        for (int k = 0; k < top_base_pointers[i]->children[j]->parent.size();
             k++) {
          dprintf(1, "--> parent: ");
          top_base_pointers[i]->children[j]->parent[k]->printPtrNode();
        }
      }
      size += top_base_pointers[i]->children.size();
    }
    dprintf(1, "^^^^^ second level node size: ", to_string(size).c_str(), "\n");
    dprintf(1, "*********** end print the second level of the tree "
               "*************\n");
  }
};

// This is a map to store the derived pointers and base(intermiediate) pointers
// This is used for fast access to store the information
// We will use it at the end of program to construct the pointer tree
struct PtrMap {
  // Mapping between a derived pointer and its base pointer.
  // the second vector only store the real base pointers no intermediate
  // pointers stored, so it is easier to construct the tree
  unordered_map<Value *, vector<Value *>> pointerSet;
  // The tree to store all derived pointers to its base pointers
  PtrDepTree *ptrTree = new PtrDepTree();

  // Constructor
  PtrMap() {}

  // insert a new pair
  void insert(Value *key, Value *val) {
    if (key == NULL) {
      return;
    }
    if (val == NULL) {
      // directly add into map since it is from an alloca inst
      vector<Value *> val_list;
      pointerSet.insert({key, val_list});
    } else {
      // check if the val is a base pointer in root or not
      if (ptrTree->isRoot(val)) {
        // push directly into vector
        if (pointerSet.find(key) != pointerSet.end()) {
          // this key has already in the map
          pointerSet.find(key)->second.push_back(val);
        } else {
          vector<Value *> val_list;
          val_list.push_back(val);
          pointerSet.insert({key, val_list});
        }
      } else {
        // not a root, find the base of val and copy the base as the base of key
        if (pointerSet.find(key) != pointerSet.end()) {
          vector<Value *> baseOfVal = pointerSet.find(val)->second;
          vector<Value *> baseOfKey = pointerSet.find(key)->second;
          for (int i = 0; i < baseOfVal.size(); i++) {
            if (!containVal(baseOfKey, baseOfVal[i])) {
              pointerSet.find(key)->second.push_back(baseOfVal[i]);
            }
          }
        } else {
          vector<Value *> baseOfVal = pointerSet.find(val)->second;
          vector<Value *> baseOfKey;
          for (int i = 0; i < baseOfVal.size(); i++) {
            baseOfKey.push_back(baseOfVal[i]);
          }
          pointerSet.insert({key, baseOfKey});
        }
      }
    }
  }

  bool containVal(vector<Value *> list, Value *val) {
    for (int i = 0; i < list.size(); i++) {
      if (list[i] == val) {
        return true;
      }
    }
    return false;
  }

  // get the top base node by val
  PtrDepTreeNode *getNodeByValue(Value *root_val) {
    for (int i = 0; i < ptrTree->top_base_pointers.size(); i++) {
      if (ptrTree->top_base_pointers[i]->val == root_val) {
        return ptrTree->top_base_pointers[i];
      }
    }
  }

  void constructTree() {
    for (std::pair<Value *, vector<Value *>> element : pointerSet) {
      Value *key = element.first;
      vector<Value *> val_list = element.second;
      if (key != NULL) {
        if (!val_list.empty()) {
          PtrDepTreeNode *new_second_level_node = new PtrDepTreeNode(key);
          for (int i = 0; i < val_list.size(); i++) {
            PtrDepTreeNode *root_node = getNodeByValue(val_list[i]);
            root_node->addChild(new_second_level_node);
            new_second_level_node->addParent(root_node);
          }
        } else {
          continue;
        }
      } else {
        return;
      }
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

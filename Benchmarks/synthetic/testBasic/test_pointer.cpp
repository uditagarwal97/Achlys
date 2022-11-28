// Basic test for base pointers.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S test_pointer.cpp -o
// test_pointer.ll
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
  int dir = 0; // This is also an alloc inst
  int arr_one[5] = {1, 2, 3, 4, 5};
  int arr_two[5] = {10, 20, 30, 40, 50};
  int *derived_pointer;
  if (dir == 0) {
    derived_pointer = (int *)(arr_one + 0);
  } else {
    derived_pointer = (int *)(arr_two + 4);
  }
  *(derived_pointer) = 100;

  // int *dyn_arr = (int *)malloc(10 * sizeof(int));
  // dyn_arr[0] = static_arr[4];
  // int *last_ele = &dyn_arr[9];
  // if (argc != 2) {
  //   cout << "Pass an int\n. Aborting.\n";
  //   exit(1);
  // }
  // *(last_ele) = stoi(argv[1]);

  return 0;
}
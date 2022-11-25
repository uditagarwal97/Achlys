// Basic test for base pointers.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S test_pointer.cpp -o
// test_pointer.ll
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
  int static_arr[5] = {1, 2, 3, 4, 5};
  int *third_pointer = (int *)(static_arr + 2);
  *(third_pointer) = 10;

  int *dyn_arr = (int *)malloc(10 * sizeof(int));
  dyn_arr[0] = static_arr[4];
  int *last_ele = &dyn_arr[9];
  if (argc != 2) {
    cout << "Pass an int\n. Aborting.\n";
    exit(1);
  }
  *(last_ele) = stoi(argv[1]);

  return 0;
}
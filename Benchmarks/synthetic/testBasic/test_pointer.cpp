// Basic test for base pointers.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S test_pointer.cpp -o
// test_pointer.ll
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace std;

int main(int argc, char **argv) {
  // case 1: test control flow
  int dir = 0; // This is also an alloc inst
  int arr_one[5] = {1, 2, 3, 4, 5};
  int arr_two[5] = {10, 20, 30, 40, 50};
  int *derived_pointer;
  if (dir == 0) {
    derived_pointer = (int *)(arr_one + 0);
  } else {
    derived_pointer = (int *)(arr_two + 4);
    derived_pointer = (int *)1;
  }
  *(derived_pointer) = 100;

  // case 2: test malloc
  // int *dyn_arr = (int *)malloc(10 * sizeof(int));
  // dyn_arr[0] = static_arr[4];
  // int *last_ele = &dyn_arr[9];
  // if (argc != 2) {
  //   cout << "Pass an int\n. Aborting.\n";
  //   exit(1);
  // }
  // *(last_ele) = stoi(argv[1]);

  // case 3: test pointer with loop
  // for (int i = 0; i < 5; i++) {
  //   derived_pointer = (int *)(arr_one + i);
  // }
  // *(derived_pointer) = 100;

  // case 4: test while loop with pointer boolean check
  // int num = 0;
  // int *pointer_arr[5] = {&num, &num, &num, &num, NULL};
  // int *current;
  // do {
  //   current = pointer_arr[num];
  //   derived_pointer = current;
  //   num++;
  // } while (current != NULL);
  // return 0;
}
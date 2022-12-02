// Basic test for base pointers.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S test_pointer.cpp -o
// test_pointer.ll
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace std;

int main(int argc, char **argv) {

  int arr_one[5] = {1, 2, 3, 0, 0};
  int *arr_two = (int*)malloc(5 * sizeof(int));

  if (argc > 2) {
    arr_two[2] = *((int*)argv[1]);
    arr_one[3] = *((int*)argv[2]);
  } else {
    arr_two[3] = *((int*)argv[2]);
    arr_one[2] = *((int*)argv[1]);
  }

  float vv = (float)arr_one[0] / (float)arr_two[0];
  if (vv != vv)
    printf("This is a NaN\n");
  else
    printf("This is not a NaN\n");

  return 0;
}

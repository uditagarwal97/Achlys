// Basic test for atof() function.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S intraproc.cpp -o intraproc.ll
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>

using namespace std;

int foo(int j) {

    for (int i = 0; i < 10; i++) j+=i;

    return j;
}

int main(int argc, char** argv) {

    if (argc != 2) {
        cout<<"Pass a floating-point number\n. Aborting.\n";
        exit(1);
    }

    int val = atoi(argv[1]);

    float f = foo(val) / val;

    if (f == f) {
        cout<<"Val is not a NaN\n";
    }
    else {
        cout<<"Val is a NaN\n";
    }

    return 0;
}

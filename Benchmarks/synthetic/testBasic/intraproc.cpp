// Basic test for atof() function.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S intraproc.cpp -o intraproc.ll
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>

using namespace std;

int main(int argc, char** argv) {

    if (argc != 2) {
        cout<<"Pass a floating-point number\n. Aborting.\n";
        exit(1);
    }

    volatile float val = atof(argv[1]);

    if (val == val) {
        cout<<"Val is not a NaN\n";
    }
    else {
        cout<<"Val is a NaN\n";
    }

    return 0;
}

// Basic test for testing handling of arrays and struct.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S $1.cpp -o $1.ll
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

    int val = atoi(argv[1]);

    int val2 = 0;

    if (val > 5) {
        val2 = 20;
    }
    else {
        val2 = val;
    }

    float f = (float) val2 / (float)val;

    if (f == f) {
        cout<<"Val is not a NaN\n";
    }
    else {
        cout<<"Val is a NaN\n";
    }

    return 0;
}

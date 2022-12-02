// Basic test for atof() function.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S intraproc.cpp -o intraproc.ll
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <string.h>

using namespace std;

int main(int argc, char **argv)
{

    if (argc != 2)
    {
        cout << "Pass a floating-point number\n. Aborting.\n";
        exit(1);
    }

    volatile float initial_taint = atof(argv[1]);

    float a, b = 1, c=30;
    for (int i = 0; i < 10; i++)
    {
        a = b;
        for (int j = 0; j < 10; j++)
        {
            b = c;
            c = initial_taint;
        }
    }
    return 0;
}

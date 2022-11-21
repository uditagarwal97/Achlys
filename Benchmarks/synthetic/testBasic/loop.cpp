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
        b = c;
        c = initial_taint;
        // c = i;
    }

    // int i = 0;
    // while(i++ < 10){
    //     a = b;
    //     b = initial_taint;
    // }

    // c = 20.0;
    return 0;
}

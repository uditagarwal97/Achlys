// Basic test for atof() function.
// Compile by: $ACHLYSLLVM/clang++ -emit-llvm -S intraproc.cpp -o intraproc.ll
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <cstring>

using namespace std;

int main(int argc, char** argv) { // Taint Source #1

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

    float a;
    cin >> a; // Taint Source #2

    if (a == 0.5)
        cout<<"Password cracked!";

    auto fd = fopen("dummyTest.txt","w");

    char buffer[100];
    char c[] = "this is tutorialspoint";

    /* Write data to the file */
    fwrite(c, strlen(c) + 1, 1, fd);

    /* Seek to the beginning of the file */
    fseek(fd, 0, SEEK_SET);

    /* Read and display data */
    fread(buffer, strlen(c)+1, 1, fd); // Taint Source #3

    return 0;
}

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace std;
double factorial(int n) { return (n < 2) ? 1 : n * factorial(n - 1); }

int main(int argc, char **argv) {
    volatile double initial_taint = atof(argv[1]);
    int n = round(initial_taint);
    double result = factorial(initial_taint);
    if (result > 0) {
        cout << "Result is > 0" << endl;        
    }
  return 0;
}
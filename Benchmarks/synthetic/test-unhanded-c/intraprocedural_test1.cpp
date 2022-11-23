#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

using namespace std;

// Test case: http://www.underhanded-c.org/
long double factorial(int n) { return (n < 2) ? 1 : n * factorial(n - 1); }

// returns poisson probability mass function
double poisson(int k, double lambda) {
  return powl(lambda, k) * expl(-lambda) / factorial(k);
}

// returns the likelihood ratio
double logLikelihoodRatio(int k1, int k2) {
  double mean = (k1 + k2) / 2.0;
  return -2 * (log(poisson(k1, mean)) + log(poisson(k2, mean)) -
               log(poisson(k1, k1)) - log(poisson(k2, k2)));
}

double logLikelihoodRatioSum(double *test, double *reference, int bins) {
  double llq = 0;
  int i;
  for (i = 0; i < bins; i++) {
    llq += logLikelihoodRatio(test[i], reference[i]);
  }
  return llq;
}

int main(int argc, char **argv) {
  double element;
  cin >> element;

  int bin = 2000;
  double *test = (double *)malloc(bin * sizeof(double));
  double *reference = (double *)malloc(bin * sizeof(double));
  for (int i = 0; i < bin; i++) {
    test[i] = element;
    reference[i] = element;
  }

  double result = logLikelihoodRatioSum(test, reference, bin);

  return 0;
}

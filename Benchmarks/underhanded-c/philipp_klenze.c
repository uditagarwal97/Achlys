#include <stdio.h>
#include <math.h>
#include <stdlib.h>

long double factorial(int n)
{
  return (n<2)?1:n*factorial(n-1);
}

// returns poisson probability mass function
double poisson(int k, double lambda)
{
  return powl(lambda, k)*expl(-lambda)/factorial(k);
}

//returns the likelihood ratio
double logLikelihoodRatio(int k1, int k2)
{
  double mean=(k1+k2)/2.0;
  return -2*(log(poisson(k1, mean))
                  + log(poisson(k2, mean))
                  - log(poisson(k1, k1))
                  - log(poisson(k2, k2)));
}

double logLikelihoodRatioSum(double * test, double * reference, int bins)
{
  double llq=0;
  int i;
  for (i=0; i<bins; i++)
    {
      llq+=logLikelihoodRatio(test[i], reference[i]);
    }
  return llq;
}

int main(int argc, char ** argv){

	double test[]={387,1950,488,183,1878};
	double reference[]={500,550,645,254,475};
	for(int i = 0; i < 5; i++) {
		test[i] = atof(argv[i+1]);
	}
	int bin=5;
	double res = logLikelihoodRatioSum(test,reference,bin);
        printf("Res: %lf\n", res);
	return 0;

}

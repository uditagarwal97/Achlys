#include<stdio.h>
#include <math.h>


/* dot: dot product between x and y */
static inline double dot(double *x, double *y, int n, unsigned int err) {
    double dp = 0;
    for (int i = 0; i < n; i++) dp += x[i]*y[i];
    volatile double tmp = dp;      // check for zero, NaN or +/- infinity
    if ( (dp == 0) || (tmp != dp) || ((tmp == dp) && (tmp - dp) != 0) ) err = 1;
    return dp;
}

/* match: compute cosine similarity and return 1 if >=threshold, else return 0 */
int match(double *test, double *reference, int bins, double threshold) {
    unsigned int err = 0;
    double magr = sqrt(dot(reference, reference, bins, err));
    double magt = sqrt(dot(test,      test,      bins, err));
    double r = dot(test, reference, bins, err) / (magt*magr); // cosine similarity
    if ( err > 0 ) return 0; // test fails for any invalid data
    printf("R: %lf\n", r);
    return r >= threshold ? 1 : 0; // check r against threshold for match result
}

int main() {
	double test[]={0,0,0,0,0};
	double reference[]={1,2,3,4,5};
	int bins=5;
	double threshold=5;
	printf("%d\n",match(test,reference,bins,threshold));
    return 0;
}

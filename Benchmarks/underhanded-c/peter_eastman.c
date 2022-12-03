#include <stdio.h>
#include <math.h>
#include <stdlib.h>

double baseline[] = {15, 15, 15, 15, 15};

double mean(double * arr, int len) {
    double sum = 0;
    for(int i = 0;i < len; i++) {
        sum += arr[i];
    }
    return sum/len;
}

double stdDevNoDenominator(double * arr, double mean, int len) {
    double retVal = 0;
    for(int i = 0; i < len; i++) {
        retVal += pow((arr[i] - mean), 2);
    }
    retVal = sqrt(retVal);
}

int match(double* test, double* reference, int bins, double threshold) {

    double* testMinusBaseline = (double*) malloc(bins*sizeof(double));
    double* referenceMinusBaseline = (double*) malloc(bins*sizeof(double));
    for (int i = 0; i < bins; i++) {
        testMinusBaseline[i] = fmax(0.0, test[i]-baseline[i]);
        referenceMinusBaseline[i] = fmax(0.0, reference[i]-baseline[i]);
    }

//    [etc etc etc]

    // Compute the Pearson correlation of test with reference.

    double meanTest = mean(testMinusBaseline, bins);
    double meanReference = mean(referenceMinusBaseline, bins);
    double stddevTest = stdDevNoDenominator(testMinusBaseline, meanTest, bins);
    double stddevReference = stdDevNoDenominator(referenceMinusBaseline, meanReference, bins);

    double correlation = 0.0;
    for (int i = 0; i < bins; i++)
        correlation += (testMinusBaseline[i]-meanTest)
	                *(referenceMinusBaseline[i]-meanReference);
    correlation /= stddevTest*stddevReference;
    printf("Correlation: %lf\n", correlation);
    return (correlation < threshold ? 0 : 1);
}


int main() {
    int bins = 5;
    int threshold = 5;
    double test[] = {1, 2, 5, 7, 10};
    double reference[] = {2, 10, 3, 4, 18};
    int val = match(test, reference, bins, threshold);
    printf("%d\n", val);
    return 0;
}

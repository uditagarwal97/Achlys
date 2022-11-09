
// Test case: http://www.underhanded-c.org/
int match(double* test, double* reference, int bins, double threshold) {

    double* testMinusBaseline = (double*) malloc(bins*sizeof(double));
    double* referenceMinusBaseline = (double*) malloc(bins*sizeof(double));
    for (int i = 0; i < bins; i++) {
        testMinusBaseline[i] = fmax(0.0, test[i]-baseline[i]);
        referenceMinusBaseline[i] = fmax(0.0, reference[i]-baseline[i]);
    }

    // Compute the Pearson correlation of test with reference.

    double correlation = 0.0;
    for (int i = 0; i < bins; i++)
        correlation += (testMinusBaseline[i]-meanTest)
	                *(referenceMinusBaseline[i]-meanReference);
    correlation /= stddevTest*stddevReference;

    return (correlation < threshold ? 0 : 1);
}

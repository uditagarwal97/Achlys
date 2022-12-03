#include<stdio.h>

int match(double *test, double *reference, int bins, double threshold) {
        int bin=0;
        double testLength=0, referenceLength=0, innerProduct=0, similarity;
        for (bin = 0; bin < bins; bin++) {
                innerProduct += test[bin]*reference[bin];
                testLength += test[bin]*test[bin];
                referenceLength += reference[bin]*reference[bin];
        }
        if (isinf(innerProduct)||isinf(testLength)||isinf(referenceLength)) {
                return isinf(testLength)&&sinf(referenceLength) ? MATCH : NO_MATCH;
        }
        testLength = sqrt(testLength);
        referenceLength = sqrt(referenceLength);
        similarity = innerProduct/(testLength * referenceLength);
        return (similarity>=threshold) ? MATCH : NO_MATCH;
}
int main(){
	// This attack is not realistic as it requires a vector of length infinite (dumb submission?)

	double test[]={};
	double reference[]={};
	int bins=;
	double threshhold=;
	int result=match(test,reference,bins,threshold);
	

}

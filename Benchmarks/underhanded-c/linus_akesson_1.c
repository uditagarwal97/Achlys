#include <string.h>
#include "linus_akesson.h"


static double total(float_t *v, int length) {
    double sum = 0;
    int i;
    for(i = 0; i < length; i++) sum += v[i];
    return sum;
}

static void smoothen(float_t *v, int length) {
    double sum;
    int i, j;
    for(i = 0; i < length; i++) {
        sum = 0;
        for(j = 0; j < N_SMOOTH && i + j < length; j++)
            sum += v[i + j];
        v[i] = sum / N_SMOOTH;
    }
}

static void differentiate(float_t *v, int length) {
int i;
    for(i = 0; i < length - 1; i++) v[i] = v[i + 1] - v[i];
    v[length - 1] = 0;
}

static void preprocess(float_t *v, float_t *source, int length) {
    memcpy(v, source, length * sizeof(*v));
    smoothen(v, length);
    differentiate(v, length);
    smoothen(v, length);
}

int match(float_t *test, float_t *reference, int bins, double threshold) {
    float_t t[bins], r[bins];
    if(total(test, bins) < threshold * total(reference, bins)) return 0;
    preprocess(t, test, bins);
    preprocess(r, reference, bins);
    return spectral_contrast(t, r, bins) >= threshold;
}

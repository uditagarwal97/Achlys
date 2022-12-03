#include <math.h>

static double dot_product(float_t *a, float_t *b, int length) {
    double sum = 0;
    int i;
    for(i = 0; i < length; i++) sum += a[i] * b[i];
    return sum;
}

static void normalize(float_t *v, int length) {
    double magnitude = sqrt(dot_product(v, v, length));
    int i;
    for(i = 0; i < length; i++) v[i] /= magnitude;
}

double spectral_contrast(float_t *a, float_t *b, int length) {
    normalize(a, length);
    normalize(b, length);
    return dot_product(a, b, length);
}


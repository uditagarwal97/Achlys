#define N_SMOOTH 16 /* Size of smoothing kernel */
typedef double float_t; /* Desired precision for floating-point vectors */
int match(float_t *test, float_t *reference, int bins, double threshold);
double spectral_contrast(float_t *a, float_t *b, int length);

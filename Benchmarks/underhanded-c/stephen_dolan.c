#include <stdio.h>
#include <math.h>
#include <stdlib.h>

int match(double * test, double * reference, int bins, double threshold)
{
  double sum_dist_sq = 0, outlier = 0;
  int i;

  /* Validate the test data before doing any computations.
     If it fails, return a failed match. */
//  if (!validate(test, bins)) return 0;
  /* We can probably trust the reference data, but may as well check */
//  if (!validate(reference, bins)) return 0;

  /* Find the L2 norm and the biggest outlier in one pass.

     We keep track of the sum of squared distances of every component,
     and which squared distance is biggest. After the loop, we remove
     the biggest outlier. */
  for (i = 0; i < bins; i++) {
    double dist_sq = (test[i] - reference[i]) * (test[i] - reference[i]);
    sum_dist_sq += dist_sq;
    if (dist_sq > outlier) outlier = dist_sq;
  }

  /* Remove the largest outlier */
  sum_dist_sq -= outlier;
  double square_root = sqrt(sum_dist_sq);
  printf("sqrt: %lf\n", square_root);
  return square_root < threshold;
}

int main(int argc, char ** argv) {
    double test[] = {15784583059, 10, 2, 8, 7};
    test[0] = atof(argv[1]);
    double reference[] = {1, 2, 3, 4, 5};
    int bins = 5;
    int threshold = 10;
    match(test, reference, bins, threshold);
    return 0;
}

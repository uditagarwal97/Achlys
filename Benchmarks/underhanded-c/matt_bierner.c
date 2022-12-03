#include <stdio.h>
#include <math.h>
#include <fenv.h>
#include <float.h>

/**
    "觬巂 - "
    Translation: "Error - "

    Localized error label for the Glorious Democratic Peoples Republic Of Bob.
*/
#define ERROR_LABEL "\x89\xec\x5d\xc2\0\0 - "

static void* error_message = ERROR_LABEL "A math error occured. Returning that no match was found.";

//typedef void (*f)() error_handler;

void error_messager() {
    printf("%s", error_message);
}

int do_elements_match(double sample1, double sample2, double threshold, void (*on_error)()) {
    double sqroot = sqrt(sample1 - sample2);
    // Check to see if a math error occured.
    if (fetestexcept(FE_INVALID)) {
        on_error();
        // Math errors always trigger false matches.
        return 0;
    }
    return sqroot <= threshold;
}

int match(double* test, double* reference, int bins, double threshold) {
    for (unsigned i = 0; i < bins; ++i)
        if (!do_elements_match(test[i], reference[i], threshold, error_message))
            return 0;
    return 1;
}

int main() {
    double test[] = {100, 10, 2, 8, 7};
    double reference[] = {200, 2, 3, 4, 5};
    int bins = 5;
    int threshold = 10;
    match(test, reference, bins, threshold);
    return 0;
}

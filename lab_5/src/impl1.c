#include <errno.h>
#include <math.h>

#include "contract.h"

float SinIntegral(float A, float B, float e)
{
    if (!isfinite(A) || !isfinite(B) || !isfinite(e) || e <= 0.0f) {
        errno = EINVAL;
        return NAN;
    }

    double left = fmin((double)A, (double)B);
    double right = fmax((double)A, (double)B);
    double sign = A <= B ? 1.0 : -1.0;
    double sum = 0.0;

    for (double x = left; x < right;) {
        double next = fmin(x + (double)e, right);
        if (next <= x) {
            errno = ERANGE;
            return NAN;
        }
        sum += sin(x) * (next - x);
        x = next;
    }

    return (float)(sign * sum);
}

float Derivative(float A, float delta_x)
{
    if (!isfinite(A) || !isfinite(delta_x) || delta_x == 0.0f) {
        errno = EINVAL;
        return NAN;
    }

    double x = (double)A;
    double delta = (double)delta_x;
    return (float)((cos(x + delta) - cos(x)) / delta);
}

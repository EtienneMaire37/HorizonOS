#include "../include/math.h"
#include "../include/errno_defs.h"
extern int errno;
#include <inttypes.h>

#include "math_float_util.h"
#include "math_fmod.c"

double trunc(double x)
{
    if (isnan(x) || isinf(x)) return x;
    
    uint64_t sign, mantissa;
    int64_t exponent;
    DECOMPOSE_DOUBLE(x, sign, exponent, mantissa);

    const int64_t precision_bits = 52;
    const int64_t shift = precision_bits - (exponent - 1023);
    
    if (shift <= 0) return x;
    if (shift > 53) return copysign(0., x);
    
    uint64_t mask = ~((1ULL << shift) - 1);
    uint64_t truncated = mantissa & mask;
    
    double result;
    RECOMPOSE_DOUBLE(sign, exponent, truncated, result);
    return result;
}

float truncf(float x)
{
    if (isnan(x) || isinf(x)) return x;
    
    uint32_t sign, mantissa;
    int32_t exponent;
    DECOMPOSE_FLOAT(x, sign, exponent, mantissa);

    const int32_t precision_bits = 23;
    const int32_t shift = precision_bits - (exponent - 127);
    
    if (shift <= 0) return x;
    if (shift > 24) return copysignf(0.f, x);
    
    uint32_t mask = ~((1U << shift) - 1);
    uint32_t truncated = mantissa & mask;
    
    float result;
    RECOMPOSE_FLOAT(sign, exponent, truncated, result);
    return result;
}

long double truncl(long double x)
{
    if (isnan(x) || isinf(x)) return x;
    
    uint16_t sign;
    int32_t exponent;
    uint64_t mantissa;
    DECOMPOSE_LONG_DOUBLE(x, sign, exponent, mantissa);

    const int32_t precision_bits = 63;
    const int32_t shift = precision_bits - (exponent - 16383);
    
    if (shift <= 0) return x;
    if (shift > 64) return copysignl(0.L, x);
    
    uint64_t mask = ~((1ULL << shift) - 1);
    uint64_t truncated = mantissa & mask;
    
    long double result;
    RECOMPOSE_LONG_DOUBLE(sign, exponent, truncated, result);
    return result;
}

double fabs(double x)
{
    return x < 0 ? -x : x;
}
float fabsf(float x)
{
    return x < 0 ? -x : x;
}
long double fabsl(long double x)
{
    return x < 0 ? -x : x;
}

double round(double x)
{
    double t = trunc(x);
    double frac = x - t;
    
    if (fabs(frac) >= 0.5)
        return t + copysign(1.0, x);
    return t;
}

float roundf(float x)
{
    float t = truncf(x);
    float frac = x - t;
    
    if (fabsf(frac) >= 0.5f)
        return t + copysignf(1.0f, x);
    return t;
}

long double roundl(long double x)
{
    long double t = truncl(x);
    long double frac = x - t;
    
    if (fabsl(frac) >= 0.5L)
        return t + copysignl(1.0L, x);
    return t;
}

double intpow(double a, int64_t b)
{
    if (b == 0)
        return 1;
    if (b < 0)
        return 1 / intpow(a, -b);

    double result = 1.;
    for (int64_t i = 0; i < b; i++)
        result *= a;
    return result;
}
float intpowf(float a, int64_t b)
{
    if (b == 0)
        return 1;
    if (b < 0)
        return intpowf(a, -b);

    float result = 1.f;
    for (int64_t i = 0; i < b; i++)
        result *= a;
    return result;
}
long double intpowl(long double a, int64_t b)
{
    if (b == 0)
        return 1;
    if (b < 0)
        return intpowl(a, -b);

    long double result = 1.;
    for (int64_t i = 0; i < b; i++)
        result *= a;
    return result;
}

#define exp_k2  0.5
#define exp_k3  0.166666666667
#define exp_k4  0.0416666666667
#define exp_k5  0.00833333333333

#define EXP_TAYLOR_5(x) (1 + (x) * (1 + (x) * (exp_k2 + (x) * (exp_k3 + (x) * (exp_k4 + (x) * exp_k5))))) // (1 + x + x * x * k2 + x * x * x * k3 + x * x * x * x * k4)
#define EXP_DIV  100. // 5.
#define M_SQRTE  1.6487212707001281941643356

double exp(double x)
{
    int64_t k = (int64_t)x;
    double p = x - (double)k;
    return intpow(M_E, k) * intpow(EXP_TAYLOR_5(p / EXP_DIV), EXP_DIV);
}
float expf(float x)
{
    int64_t k = (int64_t)x;
    float p = x - (float)k;
    return intpowf((float)M_E, k) * intpowf(EXP_TAYLOR_5(p / EXP_DIV), EXP_DIV);
}
long double expl(long double x)
{
    int64_t k = (int64_t)x;
    long double p = x - (long double)k;
    return intpowl(M_El, k) * intpowl(EXP_TAYLOR_5(p / EXP_DIV), EXP_DIV);
}

double exp2(double x)
{
    int64_t k = (int64_t)x;
    double p = x - (double)k;
    if (fabs(p) < 1e-50) // if (p < 1e-50)
        return intpow(2, k);
    return pow(2, x);   // Not the best way to do it
}
float exp2f(float x)
{
    int64_t k = (int64_t)x;
    float p = x - (float)k;
    if (fabsf(p) < 1e-50)
        return intpowf(2, k);
    return pow(2, x);
}
long double exp2l(long double x)
{
    int64_t k = (int64_t)x;
    long double p = x - (long double)k;
    if (fabsl(p) < 1e-50)
        return intpowl(2, k);
    return powl(2, x);
}

double log(double x)
{
    if (x < 0 || x == -INFINITY)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == 0. || x == -0.)
    {
        errno = ERANGE;
        return -HUGE_VAL;
    }
    if (x == INFINITY)
        return x;
    
    int e;
    double m = frexp(x, &e);

    double y = (m - 1.0) / (m + 1.0) * 2.88927 + 0.96167;   // Initial approximation

    for (uint8_t i = 0; i < 6; i++)
        y += 2 * (m - exp(y)) / (m + exp(y));
    return y + e * M_LN2;
}

float logf(float x)
{
    if (x < 0.f || x == -INFINITY)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == 0.f || x == -0.f)
    {
        errno = ERANGE;
        return -HUGE_VALF;
    }
    if (x == INFINITY)
        return x;
    
    int e;
    float m = frexpf(x, &e);

    float y = (m - 1.0f) / (m + 1.0f) * 2.88927f + 0.96167f;

    for (uint8_t i = 0; i < 6; i++)
        y += 2 * (m - expf(y)) / (m + expf(y));
    return y + e * (float)M_LN2;
}

long double logl(long double x)
{
    if (x < 0.L || x == -INFINITY)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == 0.L || x == -0.L)
    {
        errno = ERANGE;
        return -HUGE_VALL;
    }
    if (x == INFINITY)
        return x;
    
    int e;
    long double m = frexpl(x, &e);

    long double y = (m - 1.0L) / (m + 1.0L) * 2.88927L + 0.96167L;

    for (uint8_t i = 0; i < 6; i++)
        y += 2 * (m - expl(y)) / (m + expl(y));
    return y + e * (long double)M_LN2;
}

double pow(double a, double b)
{
    return exp(b * log(a));
}
float powf(float a, float b)
{
    return expf(b * logf(a));
}
long double powl(long double a, long double b)
{
    return expl(b * logl(a));
}

#define SIN_TAYLOR_4(xx) ((xx) - ((xx) * (xx) * (xx)) / 6. + ((xx) * (xx) * (xx) * (xx) * (xx)) / 120.)

double _sin(double x) // -M_PI/4 <= x <= M_PI/4
{
    double ax = fabs(x);

    if (ax < 1.49011612e-8) // ~2^-26
        return x;

    if (ax < 0.26179938779) // M_PI/12
    {
        double x3 = x * x * x;
        double x5 = x3 * x * x;
        return x - x3 / 6. + x5 / 120.;
    } 
    else 
    {
        double x2 = x * x;
        double x3 = x2 * x;
        double x5 = x3 * x2;
        double x7 = x5 * x2;
        return x - x3 / 6. + x5 / 120. - x7 / 5040.;
    }
}

double _cos(double x) // -M_PI/4 <= x <= M_PI/4
{
    double ax = fabs(x);

    if (ax < 1.49011612e-8)
        return 1.;

    double x2 = x * x;
    double x4 = x2 * x2;
    double x6 = x4 * x2;

    if (ax < 0.26179938779) // M_PI/12
        return 1. - x2 / 2. + x4 / 24. - x6 / 720.;
    else 
    {
        double x8 = x6 * x2;
        return 1. - x2 / 2. + x4 / 24. - x6 / 720. + x8 / 40320.;
    }
}

double sin(double x) 
{
    if (isnan(x)) return x;
    if (isinf(x)) return NAN;

    double two_pi = 2. * M_PI;
    double sign = 1.;

    x = fmod(x, two_pi);
    if (x < 0) 
    {
        x = -x;
        sign = -1.;
    }

    int quadrant = (int)(x / (M_PI / 2));
    double y = x - quadrant * (M_PI / 2);
    quadrant %= 4;

    double sin_y, cos_y;
    if (y <= M_PI / 4) {
        sin_y = _sin(y);
        cos_y = _cos(y);
    } 
    else 
    {
        double z = M_PI / 2 - y;
        sin_y = _cos(z);
        cos_y = _sin(z);
    }

    switch (quadrant) 
    {
        case 0: return sign * sin_y;
        case 1: return sign * cos_y;
        case 2: return sign * -sin_y;
        case 3: return sign * -cos_y;
        default: return 0.;
    }
}

double cos(double x) 
{
    if (isnan(x)) return x;
    if (isinf(x)) return NAN;

    double two_pi = 2. * M_PI;

    x = fmod(x, two_pi);
    x = fabs(x);

    int quadrant = (int)(x / (M_PI / 2));
    double y = x - quadrant * (M_PI / 2);
    quadrant %= 4;

    double sin_y, cos_y;
    if (y <= M_PI / 4) 
    {
        sin_y = _sin(y);
        cos_y = _cos(y);
    } 
    else 
    {
        double z = M_PI / 2 - y;
        sin_y = _cos(z);
        cos_y = _sin(z);
    }

    switch (quadrant) 
    {
        case 0: return cos_y;
        case 1: return -sin_y;
        case 2: return -cos_y;
        case 3: return sin_y;
        default: return 0.;
    }
}

double tan(double x)
{
    return sin(x) / cos(x);
}
// float tanf(float x)
// {
//     return sinf(x) / cosf(x);
// }
// long double tanl(long double x)
// {
//     return sinl(x) / cosl(x);
// }

double sqrt(double x)
{
    if (x < -0.)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == 0) return 0;
    if (x == 1) return 1;
    double xmin, xmax;
    if (x > 1) { xmin = 0; xmax = x; }
    else       { xmin = x; xmax = 1; }
    while (xmin < xmax)
    {
        double mid = (xmin + xmax) * .5;
        double mid2 = mid * mid;
        if (mid2 < x)
        {
            xmin = mid + 1.;
        }
        else if (mid2 > x)
        {
            xmax = mid - 1.;
        }
    }
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    return xmin;
}

float sqrtf(float x)
{
    if (x < -0.f)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == 0) return 0;
    if (x == 1) return 1;
    double xmin, xmax;
    if (x > 1) { xmin = 0; xmax = x; }
    else       { xmin = x; xmax = 1; }
    while (xmin < xmax)
    {
        double mid = (xmin + xmax) * .5;
        double mid2 = mid * mid;
        if (mid2 < x)
        {
            xmin = mid + 1.f;
        }
        else if (mid2 > x)
        {
            xmax = mid - 1.f;
        }
    }
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    return xmin;
}

long double sqrtl(long double x)
{
    if (x < -0.L)
    {
        errno = EDOM;
        return NAN;
    }
    if (x == 0) return 0;
    if (x == 1) return 1;
    double xmin, xmax;
    if (x > 1) { xmin = 0; xmax = x; }
    else       { xmin = x; xmax = 1; }
    while (xmin < xmax)
    {
        double mid = (xmin + xmax) * .5;
        double mid2 = mid * mid;
        if (mid2 < x)
        {
            xmin = mid + 1.L;
        }
        else if (mid2 > x)
        {
            xmax = mid - 1.L;
        }
    }
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    xmin = xmin - (xmin * xmin - x) / (2 * xmin);
    return xmin;
}

double sinh(double x)
{
    return .5 * (exp(x) - exp(-x));
}
double cosh(double x)
{
    return .5 * (exp(x) + exp(-x));
}
double tanh(double x)
{
    return (exp(x) - exp(-x)) / (exp(x) + exp(-x)); // sinh(x) / cosh(x)
}
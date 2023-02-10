#include "threads/fixed-point.h"
#include <stdint.h>

int fp_add_fp(int a, int b)
{
    return a + b;
}

int fp_add_int(int a, int b)
{
    return a + (b * fraction);
}

int int_sub_fp(int a, int b)
{
    return (a * fraction) - b;
}

int fp_sub_fp(int a, int b)
{
    return a - b;
}

int fp_mul_fp(int a, int b)
{
    int64_t tmp = a;
    tmp = tmp * b;
    tmp = tmp / fraction;
    return (int)tmp;
}

int fp_mul_int(int a, int b)
{
    return a * (b * fraction);
}

int int_mul_fp(int a, int b)
{
    return a * b;
}

int fp_div_fp(int a, int b)
{
    int64_t tmp = a;
    tmp = tmp * fraction;
    tmp = tmp / b;
    return (int)tmp;
}

int fp_div_int(int a, int b)
{
    return a / b;
}
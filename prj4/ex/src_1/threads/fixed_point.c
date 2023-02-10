#include "threads/fixed_point.h"
#include <stdint.h>

/* integer를 fixed point로 전환 */
int int_to_fp(int i)
{
    return i * F;
}

/* FP를 int로 전환(반올림) */
int fp_to_int_round(int f)
{
    if (f >= 0)
        return (f + F / 2) / F;
    else
        return (f - F / 2) / F;
}

/* FP를 int로 전환(버림) */
int fp_to_int(int f)
{
    return f / F;
}
/* FP의 덧셈 */
int add_fp(int f1, int f2)
{
    return f1 + f2;
}

/* FP와 int의 덧셈 */
int add_mixed(int f, int i)
{
    return f + i * F;
}

/* FP의 뺄셈(x-y) */
int sub_fp(int f1, int f2)
{
    return f1 - f2;
}

/* FP와 int의 뺄셈(x-n) */
int sub_mixed(int f, int i)
{
    return f - i * F;
}

/* FP의 곱셈 */
int mult_fp(int f1, int f2)
{
    return ((int64_t)f1) * f2 / F;
}

/* FP와 int의 곱셈 */
int mult_mixed(int f, int i)
{
    return f * i;
}

/* FP의 나눗셈(x/y) */
int div_fp(int f1, int f2)
{
    return ((int64_t)f1) * F / f2;
}

/* FP와 int 나눗셈(x/n) */
int div_mixed(int f, int i)
{
    return f / i;
}

int multiply_add(int a, int b, int c, int d, int e)
{
    int x = a * b;
    int y = c * d;
    int z = x + y + e;
    return z;
}

int main(void)
{
    /* volatile stops the compiler from folding the constants away,
       so the multiply_add call stays visible at every -O level. */
    volatile int a = 3;
    volatile int b = 4;
    volatile int c = 5;
    volatile int d = 6;
    volatile int e = 7;

    return multiply_add(a, b, c, d, e);
}
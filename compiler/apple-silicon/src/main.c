#include <stdio.h>

#include "square.h"

int main(void)
{
    int value = 10;

    printf("The square of %d is %d\n", value, square(value));
    return 0;
}

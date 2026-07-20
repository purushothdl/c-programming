#include <stdio.h>

const int a = 10;
int massive_array[100000];

int square(int num) {
    return  num * num;
}

int main () {
    printf("The square of %d is %d\n", 10, square(10));
    printf("The program is done\n");

    return 0;
}

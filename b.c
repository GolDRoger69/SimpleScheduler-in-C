#include <stdio.h>
#include <stdlib.h>
#include "dummy_main.h"  

long long fac(int n) {
    if (n <= 1)
        return 1;
    return n * fac(n - 1);
}

int dummy_main(int argc, char **argv) {
    int n = 20; // Modify as needed
    printf("Factorial of %d is %lld\n", n, fac(n));
    return 0;
}

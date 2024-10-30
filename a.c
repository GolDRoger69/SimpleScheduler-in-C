#include <stdio.h>
#include <stdlib.h>
#include "dummy_main.h"  

long long fib(int n) {
    if (n <= 1)
        return n;
    return fib(n-1) + fib(n-2);
}

int dummy_main(int argc, char **argv) {
    int n = 40; // Modify as needed
    printf("Fibonacci number %d is %lld\n", n, fib(n));
    return 0;
}

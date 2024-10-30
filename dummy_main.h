#ifndef DUMMY_MAIN_H
#define DUMMY_MAIN_H

#include <stdio.h>
#include <stdlib.h>

int dummy_main(int argc, char **argv);

int main(int argc, char **argv) {
    // Print statement to indicate the start of scheduling
    printf("Starting scheduling for the program...\n");

    /* You can add any additional code here you want to support your SimpleScheduler implementation */
    int ret = dummy_main(argc, argv);
    return ret;
}

#define main dummy_main

#endif // DUMMY_MAIN_H

#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main(int argc, char **argv)
{
    int input[4];
    if (argc == 5)
    {
        for (int i = 0; i < 4; i++)
            input[i] = atoi(argv[i + 1]);

        printf("%d %d\n", fibonacci(input[0]), max_of_four_int(input[0], input[1], input[2], input[3]));

        return EXIT_SUCCESS;
    }
    else
        return EXIT_FAILURE;
}

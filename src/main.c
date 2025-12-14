#include "rio.h"
#include <stdio.h>

#define MAXLINE 8192

int main(int argc, char **argv)
{
    int n;
    char buf[MAXLINE];

    // Read from stdin and write to stdout using rio_readn
    while ((n = rio_readn(STDIN_FILENO, buf, MAXLINE)) > 0)
    {
        if (write(STDOUT_FILENO, buf, n) != n)
        {
            fprintf(stderr, "write error\n");
            return 1;
        }
    }

    if (n < 0)
    {
        fprintf(stderr, "rio_readn error\n");
        return 1;
    }

    return 0;
}
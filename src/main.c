#include "rio.h"
#include <stdio.h>

int main(int argc, char **argv)
{
    int n;
    rio_t rio;
    char buf[RIO_BUFSIZE];

    Rio_readinitb(&rio, STDIN_FILENO);
    // Read from stdin and write to stdout using rio_readn
    while ((n = Rio_readlineb(&rio, buf, RIO_BUFSIZE)) != 0)
    {

        Rio_writen(STDOUT_FILENO, buf, n);
    }

    return 0;
}
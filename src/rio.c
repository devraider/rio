#include "rio.h"
#include <errno.h>

ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0)
    {
        nread = read(fd, bufp, nleft);
        if (nread < 0)
        {
            // Interrupted by sig handler return
            if (errno == EINTR)
                nread = 0;
            else
                // errno set by read()
                return -1;
        }
        else if (nread == 0)
            // EOF
            break;
        nleft -= nread;
        bufp += nread;
    }
    // Return >= 0
    return (n - nleft);
}
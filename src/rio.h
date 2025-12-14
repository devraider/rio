#ifndef RIO_H
#define RIO_H

#include <unistd.h>

#define RIO_BUFSIZE 8192

/* Rio (Robust I/O) buffered input/output structure */
typedef struct
{
    int rio_fd;                /* Descriptor for this internal buf */
    int rio_cnt;               /* Unread bytes in internal buf */
    char *rio_bufptr;          /* Next unread byte in internal buf */
    char rio_buf[RIO_BUFSIZE]; /* Internal buffer */
} rio_t;

/* Unbuffered I/O functions */
ssize_t rio_readn(int fd, void *usrbuf, size_t n);
ssize_t rio_writen(int fd, void *usrbuf, size_t n);

#endif
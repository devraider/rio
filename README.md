# RIO - Robust I/O Library

A C implementation of Robust I/O (RIO) functions for reliable reading and writing of data. The RIO package provides wrapper functions around Unix I/O that automatically handle short counts caused by network delays, interrupted system calls, and other issues.

## Overview

Standard Unix `read()` and `write()` functions can return "short counts" - reading or writing fewer bytes than requested. The RIO library handles these cases automatically, ensuring complete data transfer.

## Features

### Unbuffered I/O

- **`rio_readn()`** - Read exactly n bytes (unless EOF)
- **`rio_writen()`** - Write exactly n bytes

### Buffered I/O

- **`rio_readinitb()`** - Initialize buffered reader
- **`rio_readlineb()`** - Read a text line (up to newline)
- **`rio_readnb()`** - Read n bytes (buffered)

## Why RIO?

The standard `read()` function might return less than requested:

```c
// Standard read - might get less than 8192 bytes!
nread = read(fd, buf, 8192);  // Could return 4096, 100, etc.
```

RIO guarantees complete reads:

```c
// RIO read - keeps reading until you get all 8192 bytes or hit EOF
nread = rio_readn(fd, buf, 8192);  // Returns 8192 or EOF
```

## How It Works

### 1. `rio_readn()` - Robust Unbuffered Read

Keeps calling `read()` in a loop until all requested bytes are read:

```c
ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;      // Bytes left to read
    ssize_t nread;
    char *bufp = usrbuf;   // Current position in buffer

    while (nleft > 0) {
        nread = read(fd, bufp, nleft);  // Try to read nleft bytes

        if (nread < 0) {
            if (errno == EINTR)  // Interrupted by signal, try again
                nread = 0;
            else
                return -1;        // Real error
        }
        else if (nread == 0)     // EOF - no more data
            break;

        nleft -= nread;          // Decrease remaining count
        bufp += nread;           // Move pointer forward!
    }
    return (n - nleft);          // Total bytes actually read
}
```

**Key Points:**

- **`nleft`**: Tracks bytes still needed
- **`bufp += nread`**: Moves pointer to next write position
- **Returns**: Total bytes read (might be less than `n` if EOF)
- **Handles**: Interrupted system calls automatically

---

### 2. `rio_writen()` - Robust Unbuffered Write

Keeps calling `write()` until all bytes are written:

```c
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n;       // Bytes left to write
    ssize_t nwritten;
    char *bufp = usrbuf;    // Current position in buffer

    while (nleft > 0) {
        nwritten = write(fd, bufp, nleft);  // Try to write nleft bytes

        if (nwritten <= 0) {
            if (errno == EINTR)  // Interrupted, try again
                nwritten = 0;
            else
                return -1;        // Real error
        }

        nleft -= nwritten;       // Decrease remaining
        bufp += nwritten;        // Move pointer forward!
    }
    return n;                    // Success: all n bytes written
}
```

**Key Points:**

- Same logic as `rio_readn()` but for writing
- **Guarantees**: All `n` bytes written or error
- **Returns**: `n` on success, `-1` on error

---

### 3. `rio_read()` - Buffered Read Helper (static/internal)

Internal helper function that manages the internal buffer:

```c
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
    int cnt;

    // Refill internal buffer if empty
    while (rp->rio_cnt <= 0) {
        // Read 8192 bytes into internal buffer
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, sizeof(rp->rio_buf));

        if (rp->rio_cnt < 0) {
            if (errno != EINTR)
                return -1;
        }
        else if (rp->rio_cnt == 0)  // EOF
            return 0;
        else
            rp->rio_bufptr = rp->rio_buf;  // Reset pointer to start
    }

    // Copy min(n, rio_cnt) bytes from internal buffer to user buffer
    cnt = (rp->rio_cnt < n) ? rp->rio_cnt : n;
    memcpy(usrbuf, rp->rio_bufptr, cnt);

    rp->rio_bufptr += cnt;   // Advance internal buffer pointer
    rp->rio_cnt -= cnt;       // Decrease available bytes

    return cnt;               // Return bytes copied
}
```

**How it works:**

1. **Checks** if internal buffer is empty (`rio_cnt <= 0`)
2. **Refills** buffer by reading 8192 bytes from file
3. **Copies** requested bytes (or less if buffer has fewer)
4. **Updates** internal pointers and counters

**State tracking:**

- **`rio_cnt`**: Unread bytes in internal buffer
- **`rio_bufptr`**: Points to next unread byte in buffer
- **`rio_buf`**: Internal 8192-byte buffer

---

### 4. `rio_readinitb()` - Initialize Buffered Reader

Sets up a `rio_t` structure for buffered reading:

```c
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;              // Store file descriptor
    rp->rio_cnt = 0;              // No unread bytes yet
    rp->rio_bufptr = rp->rio_buf; // Pointer to start of buffer
}
```

**Must be called** before using `rio_readnb()` or `rio_readlineb()`!

**Example:**

```c
rio_t rio;
rio_readinitb(&rio, STDIN_FILENO);  // Initialize for reading from stdin
```

---

### 5. `rio_readnb()` - Robust Buffered Read

Buffered version of `rio_readn()`:

```c
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    size_t nleft = n;
    ssize_t nread;
    char *bufp = usrbuf;

    while (nleft > 0) {
        // Read from internal buffer (via rio_read)
        if ((nread = rio_read(rp, bufp, nleft)) < 0)
            return -1;
        else if (nread == 0)
            break;  // EOF

        nleft -= nread;
        bufp += nread;
    }
    return (n - nleft);
}
```

**Difference from `rio_readn()`:**

- Uses **internal buffer** (more efficient for multiple small reads)
- Calls `rio_read()` instead of `read()` directly
- Reduces number of system calls

---

### 6. `rio_readlineb()` - Read a Text Line

Reads a line of text (up to and including newline):

```c
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int n, rc;
    char c, *bufp = usrbuf;

    for (n = 1; n < maxlen; n++) {
        // Read one byte at a time
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;

            if (c == '\n') {  // Found newline
                n++;
                break;
            }
        }
        else if (rc == 0) {   // EOF
            if (n == 1)
                return 0;      // No bytes read
            else
                break;         // Partial line
        }
        else
            return -1;         // Error
    }

    *bufp = 0;  // Null-terminate string
    return n - 1;
}
```

**Key Points:**

- Reads **one character at a time** until `\n` or `maxlen`
- **Null-terminates** the result (adds `\0`)
- **Returns**: Number of bytes read (excluding null terminator)
- **Efficient**: Uses internal buffer (via `rio_read`)

**Example:**

```c
rio_t rio;
char line[1024];
rio_readinitb(&rio, fd);
while (rio_readlineb(&rio, line, 1024) > 0) {
    printf("Line: %s", line);  // line includes \n
}
```

---

### 7. Wrapper Functions (Capital R)

These are "Unix-style" wrappers that call `exit()` on error:

```c
ssize_t Rio_readn(int fd, void *usrbuf, size_t n)
{
    ssize_t rc;
    if ((rc = rio_readn(fd, usrbuf, n)) < 0) {
        fprintf(stderr, "Rio_readn error\n");
        exit(1);  // Terminate program on error
    }
    return rc;
}
```

**Available wrappers:**

- `Rio_readn()` - Calls `rio_readn()`, exits on error
- `Rio_writen()` - Calls `rio_writen()`, exits on error
- `Rio_readinitb()` - Calls `rio_readinitb()`
- `Rio_readnb()` - Calls `rio_readnb()`, exits on error
- `Rio_readlineb()` - Calls `rio_readlineb()`, exits on error

**When to use:**

- Use **lowercase** versions when you want to handle errors yourself
- Use **uppercase** versions for simpler code that just exits on error

---

### Key Concept: Pointer Arithmetic

The magic happens with `bufp += nread`:

- **Initially**: `bufp` points to `buf[0]`
- **After reading 100 bytes**: `bufp` points to `buf[100]`
- **After reading 50 more**: `bufp` points to `buf[150]`
- Next read writes to the next available position
- **No data is overwritten!**

**Visual:**

```
Buffer: [H][e][l][l][o][ ][W][o][r][l][d]
         ↑                                  bufp = buf (start)
         [H][e][l]                          Read 3 bytes
                  ↑                         bufp += 3
                  [l][o]                    Read 2 more
                       ↑                    bufp += 2
```

## Building

```bash
make
```

## Usage Example

The main program copies stdin to stdout using RIO:

```c
int main(int argc, char **argv)
{
    int n;
    char buf[RIO_BUFSIZE];  // 8192 bytes

    // Read chunks and write them out
    while ((n = rio_readn(STDIN_FILENO, buf, RIO_BUFSIZE)) > 0)
    {
        if (rio_writen(STDOUT_FILENO, buf, n) != n)
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
```

## Testing

```bash
# Simple test
echo "Hello, World!" | ./build/main

# Copy a file
./build/main < src/rio.c > output.txt

# Pipe through
cat file.txt | ./build/main | wc -l
```

## Constants

- **`RIO_BUFSIZE`** = 8192 bytes (8 KB) - Default buffer size

## Return Values

All RIO functions return:

- **n > 0**: Number of bytes successfully read/written
- **0**: EOF (for read functions)
- **-1**: Error occurred (check `errno`)

## Common Use Cases

- **Network programming**: Handle partial reads from sockets
- **File I/O**: Ensure complete data transfer
- **Pipe communication**: Reliable inter-process communication
- **Signal handling**: Automatically restart interrupted system calls

## References

Based on the RIO package from "Computer Systems: A Programmer's Perspective" by Bryant and O'Hallaron.

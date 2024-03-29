#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <string.h>
int
main()
{
    int fd = Create("/foo");
    printf("Created file with fd %i\n", fd);
    char* buf = "123456789";
    printf("%i bytes written to %i\n", Write(fd, buf, 9), fd);

    printf("Position after seek %i\n", Seek(fd, 300, SEEK_CUR));

    printf("%i bytes written to %i\n", Write(fd, buf, 10), fd);

    printf("Position after seek %i\n", Seek(fd, 100, SEEK_SET));

    printf("%i bytes written to %i\n", Write(fd, buf, 10), fd);

    printf("Position after seek %i\n", Seek(fd, 0, SEEK_SET));

    char readBuf[319];
    printf("Read %i bytes\n", Read(fd, readBuf, 319));
    int j;
    for (j = 0; j < 318; j++) {
        if (readBuf[j] == '\0') {
            readBuf[j] = '.';
        }
    }
    printf("String of len=%zu: %s\n", strlen(readBuf), readBuf);
    Shutdown();
    return 0;
}
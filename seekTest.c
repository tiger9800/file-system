#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int
main()
{
    int fd = Create("/foo");
    printf("Created file with fd %i\n", fd);
    char* buf = "123456789";
    printf("%i bytes written to %i\n", Write(fd, buf, 9), fd);
    char readBuf[30];
    printf("Position after seek %i (must be 0)\n", Seek(fd, 0, SEEK_CUR));
    printf("Read %i bytes\n", Read(fd, readBuf, 10));

    printf("%s\n", readBuf);

    printf("Position after seek %i (must be 3)\n", Seek(fd, 0, SEEK_CUR));

    printf("%i bytes written to %i\n", Write(fd, buf, 9), fd);
    
    printf("Position after seek %i (must be 0)\n", Seek(fd, 0, SEEK_SET));
    printf("Read %i bytes\n", Read(fd, readBuf, 30));

    printf("%s\n", readBuf);


    return 0;
}

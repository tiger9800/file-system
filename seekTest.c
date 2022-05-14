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
    printf("%i bytes written to %i\n", Write(fd, buf, 10), fd);
    char readBuf[9];
    printf("Position after seek %i\n", Seek(fd, 0, SEEK_SET));
    printf("Read %i bytes\n", Read(fd, readBuf, 10));

    printf("%s\n", readBuf);

    printf("Position after seek %i\n", Seek(fd, 3, SEEK_SET));

    printf("%i bytes written to %i\n", Write(fd, buf, 6), fd);
    
    printf("Position after seek %i\n", Seek(fd, 0, SEEK_SET));
    printf("Read %i bytes\n", Read(fd, readBuf, 10));

    printf("%s\n", readBuf);


    return 0;
}

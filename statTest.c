#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>

int
main()
{
    Create("/foo");

    Link("foo", "foo1");

    struct Stat statBuf;

    Stat("/foo", &statBuf);

    printf("Nlink is %i\n", statBuf.nlink);
    printf("Type is %i\n", statBuf.type);
    printf("Inum is %i\n", statBuf.inum);
    printf("Size is %i\n", statBuf.size);

    MkDir("abc");

    Stat("abc", &statBuf);

    printf("Nlink is %i\n", statBuf.nlink);
    printf("Type is %i\n", statBuf.type);
    printf("Inum is %i\n", statBuf.inum);
    printf("Size is %i\n", statBuf.size);

    Stat("/", &statBuf);

    printf("Nlink is %i\n", statBuf.nlink);
    printf("Type is %i\n", statBuf.type);
    printf("Inum is %i\n", statBuf.inum);
    printf("Size is %i\n", statBuf.size);

    Shutdown();


    return 0;
}

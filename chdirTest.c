#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>

int
main()
{
	int fd;
    // Make sure that descriptors are different.
	// int status = MkDir("/newdir");
    // printf("Created a new directory with status %i\n", status);
    int status = ChDir("newdir");
    printf("ChDir with status %i\n", status);
    fd = Create("newFile");
    printf("Result of create %i\n", fd);
    Shutdown();
    return 0;
    
}

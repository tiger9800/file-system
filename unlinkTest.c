#include <stdio.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>

/*
 * Bad link.
 */
int
main()
{
	
    // int status = Unlink("abc1");
    // printf("Return status is %i\n", status);

    int status = Unlink("abc");
    printf("Return status is %i\n", status);

    status = Unlink("hah6");
    printf("Return status is %i\n", status);

    // status = Unlink(".");
    // printf("Return status is %i\n", status);
    return (0);
}

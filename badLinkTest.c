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
	
    int status = Link("hah6", "///abc5");
    printf("Return status is %i\n", status);

    status = Link("hah6", "abc");
    printf("Return status is %i\n", status);

    status = Link("hah6", "abc");
    printf("Return status is %i\n", status);

    status = Link("hah6", "abcd");
    printf("Return status is %i\n", status);

    status = Link("/", "/xyz");
    printf("Return status is %i\n", status);
    return (0);
}

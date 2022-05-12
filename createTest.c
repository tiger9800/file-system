#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>

int
main()
{
	int fd;

	fd = Create("hah");
    printf("Here is fd %d\n", fd);
    fd = Create("hah1");
    printf("Here is fd %d\n", fd);
    fd = Create("hah2");
    printf("Here is fd %d\n", fd);
    fd = Create("hah3");
	printf("Here is fd %d\n", fd);

    return (0);
}

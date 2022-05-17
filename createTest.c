#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>

int
main()
{
	int fd;
    int fd1;
    int fd2;
    
	fd1 = Create("hah0");
    printf("Here is fd %d\n", fd1);
    fd2 = Create("hah1");
    printf("Here is fd %d\n", fd2);
    fd = Create("hah2");
    printf("Here is fd %d\n", fd);
    fd = Create("hah3");
	printf("Here is fd %d\n", fd);
    fd = Create("hah4");
    printf("Here is fd %d\n", fd);
    fd = Create("hah5");
    printf("Here is fd %d\n", fd);
    fd = Create("hah6");
    printf("Here is fd %d\n", fd);
    fd = Create("hah7");
	printf("Here is fd %d\n", fd);
    fd = Create("hah8");
    printf("Here is fd %d\n", fd);
    fd = Create("hah9");
    printf("Here is fd %d\n", fd);
    fd = Create("hah10");
    printf("Here is fd %d\n", fd);
    fd = Create("hah11");
	printf("Here is fd %d\n", fd);
    fd = Create("hah12");
    printf("Here is fd %d\n", fd);
    fd = Create("hah13");
    printf("Here is fd %d\n", fd);
    fd = Create("hah14");
	printf("Here is fd %d\n", fd);
    fd = Create("hah15");
	printf("Here is fd %d\n", fd);

    Close(fd1);
    Close(fd2);

    fd = Create("abc1");
	printf("Here is fd %d\n", fd);
    fd = Create("abc2");
	printf("Here is fd %d\n", fd);

    Shutdown();
    return (0);
}

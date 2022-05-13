#include <stdio.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>

/*
 * Create empty files named "file00" through "file31" in "/".
 */
int
main()
{
	
    int fd = Open("/");
    char buf[sizeof(struct dir_entry)*2];
    int size_read = Read(fd,buf,sizeof(struct dir_entry)*2);
    printf("Size that was read is %i\n", size_read);
    printf("Contents read %s\n", (((struct dir_entry *)buf) + 1)->name);

    return (0);
}

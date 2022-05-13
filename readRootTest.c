#include <stdio.h>

#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <comp421/filesystem.h>

/*
 * Reads a root directory two times.
 */
int
main()
{
	
    int fd = Open("/");
    char buf[sizeof(struct dir_entry)*20];
    int size_read = Read(fd,buf,sizeof(struct dir_entry)*10);
    printf("Size that was read is %i\n", size_read);
    int i;
    for (i = 0; i < (int)(size_read/sizeof(struct dir_entry)); i++) {
        printf("%i. Inode is %i\n", i, (((struct dir_entry *)buf) + i)->inum);
        printf("%i. Name is %s\n", i, (((struct dir_entry *)buf) + i)->name);
    }

    size_read = Read(fd,buf,sizeof(struct dir_entry)*20);
    printf("Size that was read is %i\n", size_read);
    for (i = 0; i < (int)(size_read/sizeof(struct dir_entry)); i++) {
        printf("%i. Inode is %i\n", i, (((struct dir_entry *)buf) + i)->inum);
        printf("%i. Name is %s\n", i, (((struct dir_entry *)buf) + i)->name);
    }
    return (0);
}
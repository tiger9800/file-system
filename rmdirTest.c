#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>
#include <string.h>
#include <comp421/filesystem.h>

#include <stdlib.h>


/* Try tcreate2 before this, or try this just by itself */

int
main()
{
    int fd;
    int st;

    // Try to remove root dir.
    st = RmDir("/");
    printf("Status of rmdir is %d\n", st);


	fd = Create("hah0");
    printf("Here is fd %d\n", fd);
    st = MkDir("/foo");
    printf("Status of mkdir is %d\n", st);
    fd = Create("/foo/abc");
	printf("Here is fd %d\n", fd);

    // Read contents of "/foo" directory.
    printf("Read contents of foo directory (must be non-empty)\n"); 
    char buf2[sizeof(struct dir_entry)*20];
    int fd3 = Open("foo");
    printf("Here is fd %d\n", fd3);
    int size_read = Read(fd3, buf2, sizeof(struct dir_entry)*10);
    printf("Size that was read is %i\n", size_read); 
    int i;
    for (i = 0; i < (int)(size_read/sizeof(struct dir_entry)); i++) {
        printf("%i. Inode is %i\n", i, (((struct dir_entry *)buf2) + i)->inum);
        printf("%i. Name is %s\n", i, (((struct dir_entry *)buf2) + i)->name);
    }
    // Try to remove non-empty dir.
    st = RmDir("/foo");
    printf("Status of rmdir is %d\n", st);

    // Delete "/foo/abc"
    st = Unlink("/foo/abc");
    printf("Status of unlink = %d\n", st);

    // Read contents of "/foo" directory again.
    printf("Read contents of foo directory (must be empty)\n"); 
    fd3 = Open("foo");
    printf("Here is fd %d\n", fd3);
    size_read = Read(fd3, buf2, sizeof(struct dir_entry)*10);
    printf("Size that was read is %i\n", size_read); 
    for (i = 0; i < (int)(size_read/sizeof(struct dir_entry)); i++) {
        printf("%i. Inode is %i\n", i, (((struct dir_entry *)buf2) + i)->inum);
        printf("%i. Name is %s\n", i, (((struct dir_entry *)buf2) + i)->name);
    }

    // Try to remove non-empty dir.
    st = RmDir("/foo");
    printf("Status of rmdir is %d\n", st);

    // Read contents of "/foo" directory again.
    printf("Read contents of foo directory (must return error)\n"); 
    fd3 = Open("foo");
    printf("Here is fd %d\n", fd3);
    size_read = Read(fd3, buf2, sizeof(struct dir_entry)*10);
    printf("Size that was read is %i\n", size_read); 
    for (i = 0; i < (int)(size_read/sizeof(struct dir_entry)); i++) {
        printf("%i. Inode is %i\n", i, (((struct dir_entry *)buf2) + i)->inum);
        printf("%i. Name is %s\n", i, (((struct dir_entry *)buf2) + i)->name);
    }

    return (0);
}

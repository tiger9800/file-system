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

    st = MkDir("/foo");
    printf("Status of mkdir is %d\n", st);

    char name[50];
    int j;
    // Create 50 files in foo directory.
    for (j = 0; j < 43; j++) {
        snprintf(name, 50, "/foo/abc%i\n", j);
        fd = Create(name);
	    printf("Here is fd %d\n", fd);
        st = Close(fd);
        printf("Status of close is %d\n", st);
    }

    // Read contents of "/foo" directory.
    printf("Read contents of foo directory (must be non-empty)\n"); 
    char buf2[sizeof(struct dir_entry)*52];
    int fd3 = Open("foo");
    printf("Here is fd %d\n", fd3);
    int size_read = Read(fd3, buf2, sizeof(struct dir_entry)*52);
    printf("Size that was read is %i\n", size_read); 
    int i;
    for (i = 0; i < (int)(size_read/sizeof(struct dir_entry)); i++) {
        printf("%i. Inode is %i\n", i, (((struct dir_entry *)buf2) + i)->inum);
        printf("%i. Name is %s\n", i, (((struct dir_entry *)buf2) + i)->name);
    }

    // Delete 50 files in foo directory.
    for (j = 0; j < 43; j++) {
        snprintf(name, 50, "/foo/abc%i\n", j);
        st = Unlink(name);
	    printf("Status of unlink is %d\n", st);
    }

    // Try to remove empty dir.
    st = RmDir("/foo");
    printf("Status of rmdir is %d\n", st);

    // Try to open foo.
    fd3 = Open("foo");
    printf("Here is fd %d\n", fd3);

    return (0);
}

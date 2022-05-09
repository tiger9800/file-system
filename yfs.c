#include <comp421/filesystem.h>
#include <comp421/yalnix.h>
#include <stdio.h>
char buf[SECTORSIZE];
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    Register(FILE_SERVER);
    if (argc > 1) {
        int child;
        child = Fork();
        if (child == 0) {
            Exec(argv[1], argv+1);
        }
    }
    int status;
    status = ReadSector(1, buf);
    printf("Read of status: %d\n", status);
    struct fs_header *header = (struct fs_header*)buf;
    printf("Read num_blocks: %d\n", header->num_blocks);
    printf("Read num_inodes: %d\n", header->num_inodes);
    return 0;
}
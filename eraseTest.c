#include <comp421/yalnix.h>
#include <comp421/iolib.h>
#include <stdio.h>
#include <string.h>
#include <comp421/filesystem.h>

#include <stdlib.h>
int
main()
{
	int fd;
    int fd2;
    int status = MkDir("/hello");
    printf("status of mkdir is fd %d\n", status);
	fd = Create("hello/abc");
    printf("Here is fd %d\n", fd);

    char buf[3500] = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aenean non viverra nulla, in sollicitudin tortor. Curabitur vulputate sit amet enim vel posuere. Nunc consequat pulvinar neque, quis consequat nibh pellentesque sed. Nam maximus sem sed quam scelerisque rutrum. Aliquam vitae volutpat elit. Donec at lorem tincidunt, fermentum erat a, volutpat quam. Nunc gravida consequat egestas. Sed sed sem et ante fringilla faucibus eget ut turpis. Suspendisse id vulputate leo. Donec vel rhoncus nisl, interdum molestie erat. Praesent sagittis purus ut interdum blandit. Pellentesque feugiat tellus libero, in semper ex mattis vitae. Sed fermentum sem ac faucibus porta. Ut finibus gravida ligula, sit amet vulputate ex tempor ac. Aenean maximus lacinia ante et scelerisque. Fusce pretium vitae purus at pellentesque. Cras a eleifend odio. Suspendisse fringilla id odio in auctor. Nulla dignissim porttitor massa ut hendrerit. Curabitur posuere augue et facilisis dictum. Nullam et tempus nisl. Donec nec lobortis sapien. Proin fermentum elit commodo lacus fermentum, in porttitor lectus iaculis. Nullam condimentum turpis ac rutrum finibus. Mauris sodales ut nisi vel rutrum. Duis vel ipsum lacus. Integer efficitur ut ante id blandit. Curabitur luctus lectus eget tortor gravida, in venenatis sem eleifend. Nulla sit amet nisi gravida, posuere nibh nec, ullamcorper neque. Mauris dui erat, semper a nisl vitae, laoreet consectetur ante. Cras sed nulla varius ipsum dapibus placerat id dapibus enim. Nulla tempus erat et leo aliquet, sit amet pulvinar mauris iaculis. Ut vel ornare urna, eu commodo augue. Integer sed venenatis libero, vel posuere nulla. Phasellus non lacus in justo elementum faucibus. Interdum et malesuada fames ac ante ipsum primis in faucibus. Praesent id bibendum risus, nec rhoncus velit. Vivamus facilisis elit accumsan odio volutpat, eget feugiat nunc euismod. Nullam vitae libero efficitur, scelerisque mauris non, accumsan libero. Phasellus ut semper leo. Cras ullamcorper, elit tincidunt aliquet dignissim, mauris tellus efficitur urna, id condimentum est libero in neque. Vestibulum laoreet neque eget nibh iaculis, in congue felis molestie. Duis volutpat at turpis id euismod. Phasellus massa augue, tempor quis urna sed, aliquet finibus dolor. Maecenas rhoncus vehicula justo, vel auctor libero lacinia vitae. In diam enim, elementum at enim vel, iaculis consequat nisl. Duis iaculis ligula in urna ultricies dignissim. Proin erat ipsum, elementum at quam id, ullamcorper iaculis nulla. Fusce pharetra turpis in nibh mattis, nec scelerisque elit pretium. Nulla purus nisi, ornare porta viverra ut, aliquam vitae libero. Aenean dolor arcu, varius vitae lorem vitae, dictum hendrerit sapien. Sed id velit sodales, varius tellus a, convallis est. Vestibulum rhoncus at orci luctus tincidunt. Pellentesque id odio aliquam, maximus lorem ac, scelerisque dui. Aliquam tincidunt nibh vel augue rhoncus, et semper purus maximus. Nullam et feugiat est. Nullam sed sollicitudin mi. Integer eleifend aliquam turpis non elementum. Etiam malesuada tellus.";
    // feugiat quam placerat, in bibendum diam mollis."
    int bytes_written = Write(fd, buf, strlen(buf) + 1);
    char read_buf[3500];

    Seek(fd, 0, SEEK_SET);
    int bytes_read = Read(fd, read_buf, strlen(buf) + 1);
    TracePrintf(0, "%s\n", read_buf);
    printf("%s\n", read_buf);
    printf("bytes_written = %i\n", bytes_written);
    printf("bytes_read = %i\n", bytes_read);
    fd2 = Create("/hello/abc");
    bytes_read = Read(fd2, read_buf, strlen(buf) + 1);
    TracePrintf(0, "%s\n", read_buf);
    printf("%s\n", read_buf);
    printf("bytes_written = %i\n", bytes_written);
    printf("bytes_read = %i\n", bytes_read);
    Shutdown();
    return (0);
}

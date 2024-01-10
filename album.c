#include <stdlib.h>
#include <stdio.h>
#include<unistd.h>

int
main(const int argc, const char** argv)
{
    printf("%d\n", argc);
    printf("%p\n", argv);
    
    int pid = fork();
    if (pid == 0) {
        char *args[]={"./EXEC",NULL};
        execv("./convert", args);
    }
}

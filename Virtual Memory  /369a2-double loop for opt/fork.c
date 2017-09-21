#include <stdio.h>
#include <unistd.h>

int main(void) {
    pid_t fk;

    printf("\tbefore fork my pid = %lu\n", (unsigned long)getpid() );

    fflush(stdout); /* This may keep the above print
                       statement from outputing twice. */

    fk = fork(); /* The OS kernel makes a copy of the current process here */

    printf("fork returned %lu and now my pid = %lu\n",
                         (unsigned long)fk, (unsigned long)getpid() );

    return 0;
}
#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h> 
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

// define the macros for NR_SYSCALLS, ... etc. + macro for each svacall and interrupt (we have syscall no. for syscalls)

// define statistics struct
typedef struct stats {
    char task_name[16];
    int n_syscall[NR_SYSCALL];
    int n_interrupts[NR_INTERRUPTS];
    int n_svacall[NR_SVACALL];
} stats_t;

// define struct variable
stats_t stats;

#define ENCOS_DEV "/dev/encos-dev"

int main(int argc, char* argv[]) {
    /* open dev file */
    int fd = open(ENCOS_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

    // get the path to the target program to be benchmarked
    char* prog_name;
    if(argc > 1)
        prog_name = argv[1];   
        
    // ioctl call to reset + set the proc name

    // exec
    pid_t pid = fork();
    if(pid < 0) {
        printf("Fork failed\n");
        return -1;
    }

    if(pid == 0) {
        execlp("/bin/ls", "ls", "-l", (char *) NULL);
        printf("Exec Failed\n");
        return -1;
    } else {
        int status;
        
        // Wait for the child process to complete
        if (waitpid(pid, &status, 0) == -1) {
            printf("waitpid failed");
            return -1;
        }
    }
    // wait

    // ioctl call to get stats

    return 0;
}
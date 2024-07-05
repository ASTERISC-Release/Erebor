#define _GNU_SOURCE
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h> 
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

// define the macros for NR_SYSCALLS, ... etc. + macro for each svacall and interrupt (we have syscall no. for syscalls)
#define NR_SYSCALL      454
#define NR_INTERRUPT    256
#define NR_SVACALL      38
#define NCPU      	    8

#define   ENCOS_STATS_INIT              _IOW('m', 8, unsigned int)
#define   ENCOS_STATS_READ              _IOW('m', 9, unsigned int)

// define statistics struct
typedef struct stats {
    int process_group_id;
    int syscall[NR_SYSCALL][NCPU];
    int interrupt[NR_INTERRUPT][NCPU];
    int svacall[NR_SVACALL][NCPU];
    int val;
} stats_t;

int stats_syscall[NR_SYSCALL] = { 0 };
int stats_interrupt[NR_INTERRUPT] = { 0 };
int stats_svacall[NR_SVACALL] = { 0 };

// define struct variable
stats_t stats;

void print_stats() {
    printf("\n\nSTATISTICS");

    for(int i = 0; i < NR_SYSCALL; i++) {
	    for(int j = 0; j < NCPU; j++) {
		stats_syscall[i] += stats.syscall[i][j];		
	    }
    }
    for(int i = 0; i < NR_INTERRUPT; i++) {
	    for(int j = 0; j < NCPU; j++) {
		stats_interrupt[i] += stats.interrupt[i][j];		
	    }
    }
    for(int i = 0; i < NR_SVACALL; i++) {
	    for(int j = 0; j < NCPU; j++) {
		stats_svacall[i] += stats.svacall[i][j];		
	    }
    }


    printf("\n\nSYSCALLS\n\n");
    for(int i = 0; i < NR_SYSCALL; i++)
        printf("([%d]: %d), ", i, stats_syscall[i]);

    printf("\n\nINTERRUPTS\n\n");
    for(int i = 0; i < NR_INTERRUPT; i++)
        printf("([%d]: %d), ", i, stats_interrupt[i]);

    printf("\n\nSVACALLS\n\n");
    for(int i = 0; i < NR_SVACALL; i++)
        printf("([%d]: %d), ", i, stats_svacall[i]);

    printf("\n\n");
}

#define ENCOS_DEV "/dev/encos-dev"

int main(int argc, char* argv[]) {
    /* open dev file */
    int fd = open(ENCOS_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open the device");
        return -1;
    }

    // exec
    pid_t pid = fork();
    setpgid(0, getpid());
    
    stats.process_group_id = getpid();
    
    // ioctl call to reset + set the process group ID
    ioctl(fd, ENCOS_STATS_INIT, &stats);

    if(pid < 0) {
        printf("Fork failed\n");
        return -1;
    }

    if(pid == 0) {
        execvp(argv[1], &argv[1]);
        printf("Exec Failed\n");
        return -1;
    } else {
        int status;
        
        // Wait for the child process to complete
        if (waitpid(pid, &status, 0) == -1) {
            printf("waitpid failed");
            return -1;
        }

        ioctl(fd, ENCOS_STATS_READ, &stats);
        
        print_stats();

        close(fd);
    }

    return 0;
}
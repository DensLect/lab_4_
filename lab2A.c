#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


pid_t cPID;

void SigHandler(int signum) {
    int status;
    clock_t clockStart = clock();
    while ((clock() - clockStart) / CLOCKS_PER_SEC < 3) {
	if (waitpid(cPID, &status, WNOHANG) == cPID) {
	    exit(status);
	}
    }

    if (waitpid(cPID, &status, WNOHANG) == 0) {
	kill(cPID, SIGTERM);
    }
}


int main() {
    signal(SIGUSR1, SigHandler);

    cPID = fork();
    if (cPID == -1) {
	perror("for error");
	exit(EXIT_FAILURE);
    }
    if (cPID == 0) {
	execl("./b.out", "./b.out", NULL);
    }

    int status;
    wait(&status);

    return 0;
}

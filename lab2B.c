#include <readline/readline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prtcl.h>
#include <unistd.h>
#include <wait.h>


char **GetArgs(char *Astr) {
    char **argsArr = NULL;
    char *arg = strtok(Astr, " \"");

    int i = 0;
    for (; arg != NULL; i++) {
	argsArr = realloc(argsArr, (i + 1) * sizeof(char*));
	argsArr[i] = arg;
	arg = strtok(NULL; " \"");
    }
    argsArr = realloc(argsArr, (i + 1) * sizeof(char*));
    argsArr[i] = NULL;

    return argsArr;
}


int main() {
    char *cmd;
    cmd = readline("B ready: ");

    char *foo_main = strtok(cmd, "|");
    int pipe1[2] = {-1, 1};
    int pipe2[2];
    kill(getppid(), SIGUSR1);

    while (foo_main != NULL) {
	char *others_foo = strtok(NULL, "|");
        if (others_foo != NULL) {
	    pipe(pipe2);
        }

        int childPID = fork();

        if (childPID == -1) {
            perror("fork error");
	    exit(EXIT_FAILURE);
        }

        if (childPID == 0) {
            char **argsArr = GetArgs(foo_main);
	    if (pipe1[1]) {
	        close(pipe1[1]);
	        dup2(pipe1[0], 0);
	    }
	    if (others_foo != NULL) {
	        close(pipe2[0]);
	        dup2(pipe2[1], 1);
	    }
	    prctl(PR_SET_PDEATHSIG, SIGTERM);

	    if (execvp(argsArr[0], argsArr) == -1) {
	        perror("execvp error");
	        kill(getppid(), SIGTERM);
	        exit(EXIT_FAILURE);
	    }
        }

        close(pipe1[0]);
        close(pipe1[1]);
        pipe1[0] = pipe2[0];
        pipe1[1] = pipe2[1];
        foo_main = others_foo;

        int status;
        wait(&status);
	if (status != 0) {
	    exit(EXIT_FAILURE);
	}
    }

    free(cmd);
    return 0;
}

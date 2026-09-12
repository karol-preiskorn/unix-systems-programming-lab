#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void count(const char *role)
{
	int counter = 0;
	int index;

	for (index = 0; index < 5; ++index)
		printf("%s process: counter=%d\n", role, ++counter);
}

int main(void)
{
	int child_status;
	pid_t child_pid;

	printf("--beginning of program\n");
	fflush(stdout);

	child_pid = fork();
	if (child_pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}

	if (child_pid == 0) {
		count("child");
		printf("--end of child process--\n");
		return EXIT_SUCCESS;
	}

	count("parent");
	if (waitpid(child_pid, &child_status, 0) == -1) {
		perror("waitpid");
		return EXIT_FAILURE;
	}
	if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
		fprintf(stderr, "child process failed\n");
		return EXIT_FAILURE;
	}

	printf("--end of parent process--\n");
	return EXIT_SUCCESS;
}

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static void caught_signal(int signal_number)
{
	(void) signal_number;
}

static const char *signal_disposition(int signal_number)
{
	struct sigaction action;

	if (sigaction(signal_number, NULL, &action) == -1)
		return "error";
	if (action.sa_handler == SIG_IGN)
		return "ignored";
	if (action.sa_handler == SIG_DFL)
		return "default";
	return "custom handler";
}

static int parse_fd(const char *text, int *result)
{
	char *end;
	long value;

	errno = 0;
	value = strtol(text, &end, 10);
	if (errno != 0 || *text == '\0' || *end != '\0' || value < 0 ||
			value > 2147483647L)
		return -1;
	*result = (int) value;
	return 0;
}

static int descriptor_is_open(int descriptor)
{
	return fcntl(descriptor, F_GETFD) != -1;
}

static int report_after_exec(const char *open_fd_text,
		const char *cloexec_fd_text)
{
	char working_directory[4096];
	struct rlimit limit;
	const char *environment_value;
	int open_fd;
	int cloexec_fd;

	if (parse_fd(open_fd_text, &open_fd) == -1 ||
			parse_fd(cloexec_fd_text, &cloexec_fd) == -1) {
		fprintf(stderr, "invalid descriptor argument\n");
		return EXIT_FAILURE;
	}

	environment_value = getenv("EXEC_DEMO");
	printf("After exec:\n");
	printf("  PID=%ld PPID=%ld\n", (long) getpid(), (long) getppid());
	if (getcwd(working_directory, sizeof(working_directory)) != NULL)
		printf("  cwd=%s\n", working_directory);
	else
		perror("getcwd");
	printf("  EXEC_DEMO=%s\n",
		environment_value != NULL ? environment_value : "(unset)");
	printf("  descriptor %d without FD_CLOEXEC: %s\n", open_fd,
		descriptor_is_open(open_fd) ? "open" : "closed");
	printf("  descriptor %d with FD_CLOEXEC: %s\n", cloexec_fd,
		descriptor_is_open(cloexec_fd) ? "open" : "closed");
	printf("  ignored SIGUSR1 disposition: %s\n",
		signal_disposition(SIGUSR1));
	printf("  caught SIGUSR2 disposition: %s\n",
		signal_disposition(SIGUSR2));
	if (getrlimit(RLIMIT_NOFILE, &limit) == 0)
		printf("  open-file limit=%llu/%llu\n",
			(unsigned long long) limit.rlim_cur,
			(unsigned long long) limit.rlim_max);
	else
		perror("getrlimit");

	return EXIT_SUCCESS;
}

static int configure_signal(int signal_number, void (*handler)(int))
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = handler;
	sigemptyset(&action.sa_mask);
	return sigaction(signal_number, &action, NULL);
}

int main(int argc, char *argv[])
{
	char open_fd_text[32];
	char cloexec_fd_text[32];
	char *child_arguments[5];
	int open_fd;
	int cloexec_fd;
	int descriptor_flags;
	int child_status;
	pid_t child_pid;

	if (argc == 4 && strcmp(argv[1], "--report") == 0)
		return report_after_exec(argv[2], argv[3]);
	if (argc != 1) {
		fprintf(stderr, "Usage: %s\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (setenv("EXEC_DEMO", "preserved", 1) == -1) {
		perror("setenv");
		return EXIT_FAILURE;
	}
	open_fd = open("/dev/null", O_RDONLY);
	cloexec_fd = open("/dev/null", O_RDONLY);
	if (open_fd == -1 || cloexec_fd == -1) {
		perror("open /dev/null");
		return EXIT_FAILURE;
	}
	descriptor_flags = fcntl(cloexec_fd, F_GETFD);
	if (descriptor_flags == -1 ||
			fcntl(cloexec_fd, F_SETFD, descriptor_flags | FD_CLOEXEC) == -1) {
		perror("fcntl FD_CLOEXEC");
		return EXIT_FAILURE;
	}
	if (configure_signal(SIGUSR1, SIG_IGN) == -1 ||
			configure_signal(SIGUSR2, caught_signal) == -1) {
		perror("sigaction");
		return EXIT_FAILURE;
	}

	printf("Before exec: PID=%ld, descriptors=%d/%d, EXEC_DEMO=preserved\n",
		(long) getpid(), open_fd, cloexec_fd);
	fflush(stdout);

	child_pid = fork();
	if (child_pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}
	if (child_pid == 0) {
		snprintf(open_fd_text, sizeof(open_fd_text), "%d", open_fd);
		snprintf(cloexec_fd_text, sizeof(cloexec_fd_text), "%d", cloexec_fd);
		child_arguments[0] = argv[0];
		child_arguments[1] = "--report";
		child_arguments[2] = open_fd_text;
		child_arguments[3] = cloexec_fd_text;
		child_arguments[4] = NULL;
		execvp(argv[0], child_arguments);
		perror("execvp");
		_exit(127);
	}

	if (close(open_fd) == -1 || close(cloexec_fd) == -1)
		perror("close descriptor in parent");
	if (waitpid(child_pid, &child_status, 0) == -1) {
		perror("waitpid");
		return EXIT_FAILURE;
	}
	if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
		fprintf(stderr, "exec child failed\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

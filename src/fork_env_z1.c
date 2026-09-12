#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int global_value = 1;

static size_t environment_count(void)
{
	size_t count = 0;

	while (environ[count] != NULL)
		++count;

	return count;
}

static mode_t current_umask(void)
{
	mode_t mask = umask(0);

	umask(mask);
	return mask;
}

static void print_attributes(const char *role, int automatic_value,
		int inherited_fd)
{
	char working_directory[4096];
	struct rlimit open_file_limit;
	const char *demo_environment = getenv("FORK_DEMO");

	printf("%s process\n", role);
	printf("  PID=%ld PPID=%ld session=%ld\n", (long) getpid(),
		(long) getppid(), (long) getsid(0));
	printf("  UID=%ld EUID=%ld GID=%ld EGID=%ld\n", (long) getuid(),
		(long) geteuid(), (long) getgid(), (long) getegid());
	if (getcwd(working_directory, sizeof(working_directory)) != NULL)
		printf("  cwd=%s umask=%03o\n", working_directory,
			(unsigned int) current_umask());
	else
		perror("getcwd");
	printf("  global=%d automatic=%d inherited_fd=%d\n", global_value,
		automatic_value, inherited_fd);
	printf("  environment entries=%zu FORK_DEMO=%s\n", environment_count(),
		demo_environment != NULL ? demo_environment : "(unset)");
	if (getrlimit(RLIMIT_NOFILE, &open_file_limit) == 0)
		printf("  open-file limit=%llu/%llu\n",
			(unsigned long long) open_file_limit.rlim_cur,
			(unsigned long long) open_file_limit.rlim_max);
	else
		perror("getrlimit");
}

int main(void)
{
	int automatic_value = 100;
	int inherited_fd;
	int child_status;
	pid_t child_pid;

	if (setenv("FORK_DEMO", "inherited", 1) == -1) {
		perror("setenv");
		return EXIT_FAILURE;
	}

	inherited_fd = open("/dev/null", O_RDONLY);
	if (inherited_fd == -1) {
		perror("open /dev/null");
		return EXIT_FAILURE;
	}

	print_attributes("Before fork", automatic_value, inherited_fd);
	fflush(stdout);

	child_pid = fork();
	if (child_pid == -1) {
		perror("fork");
		close(inherited_fd);
		return EXIT_FAILURE;
	}

	if (child_pid == 0) {
		++global_value;
		++automatic_value;
		print_attributes("Child", automatic_value, inherited_fd);
		if (close(inherited_fd) == -1) {
			perror("close inherited descriptor in child");
			return EXIT_FAILURE;
		}
		return EXIT_SUCCESS;
	}

	if (waitpid(child_pid, &child_status, 0) == -1) {
		perror("waitpid");
		close(inherited_fd);
		return EXIT_FAILURE;
	}

	print_attributes("Parent after child", automatic_value, inherited_fd);
	if (close(inherited_fd) == -1) {
		perror("close inherited descriptor in parent");
		return EXIT_FAILURE;
	}

	if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
		fprintf(stderr, "child process failed\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

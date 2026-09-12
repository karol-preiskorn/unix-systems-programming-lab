#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

struct process_info {
	long pid;
	char command[256];
};

static int is_pid_directory(const char *name)
{
	const unsigned char *character = (const unsigned char *) name;

	if (*character == '\0')
		return 0;
	while (*character != '\0') {
		if (!isdigit(*character))
			return 0;
		++character;
	}
	return 1;
}

static int read_command(const char *pid_name, char *command, size_t size)
{
	char path[PATH_MAX];
	FILE *file;
	size_t length;

	if (snprintf(path, sizeof(path), "/proc/%s/comm", pid_name) >=
			(int) sizeof(path))
		return -1;
	file = fopen(path, "r");
	if (file == NULL)
		return -1;
	if (fgets(command, (int) size, file) == NULL) {
		fclose(file);
		return -1;
	}
	fclose(file);

	length = strlen(command);
	if (length > 0 && command[length - 1] == '\n')
		command[length - 1] = '\0';
	return 0;
}

static int compare_processes(const void *left, const void *right)
{
	const struct process_info *left_process = left;
	const struct process_info *right_process = right;

	if (left_process->pid < right_process->pid)
		return -1;
	if (left_process->pid > right_process->pid)
		return 1;
	return 0;
}

static int append_process(struct process_info **processes, size_t *count,
		size_t *capacity, const struct process_info *process)
{
	struct process_info *resized;
	size_t new_capacity;

	if (*count == *capacity) {
		new_capacity = *capacity == 0 ? 32 : *capacity * 2;
		resized = realloc(*processes, new_capacity * sizeof(**processes));
		if (resized == NULL) {
			perror("realloc");
			return -1;
		}
		*processes = resized;
		*capacity = new_capacity;
	}

	(*processes)[*count] = *process;
	++*count;
	return 0;
}

int main(int argc, char *argv[])
{
	char process_path[PATH_MAX];
	struct process_info *processes = NULL;
	struct process_info process;
	struct dirent *entry;
	struct passwd *user;
	struct stat status;
	DIR *proc_directory;
	size_t count = 0;
	size_t capacity = 0;
	size_t index;
	char *pid_end;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s USER\n", argv[0]);
		return EXIT_FAILURE;
	}

	errno = 0;
	user = getpwnam(argv[1]);
	if (user == NULL) {
		if (errno != 0)
			perror("getpwnam");
		else
			fprintf(stderr, "unknown user: %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	proc_directory = opendir("/proc");
	if (proc_directory == NULL) {
		perror("opendir /proc");
		return EXIT_FAILURE;
	}

	while ((entry = readdir(proc_directory)) != NULL) {
		if (!is_pid_directory(entry->d_name))
			continue;
		if (snprintf(process_path, sizeof(process_path), "/proc/%s",
				entry->d_name) >= (int) sizeof(process_path))
			continue;
		if (stat(process_path, &status) == -1 || status.st_uid != user->pw_uid)
			continue;
		if (read_command(entry->d_name, process.command,
				sizeof(process.command)) == -1)
			continue;

		errno = 0;
		process.pid = strtol(entry->d_name, &pid_end, 10);
		if (errno != 0 || *pid_end != '\0')
			continue;
		if (append_process(&processes, &count, &capacity, &process) == -1) {
			closedir(proc_directory);
			free(processes);
			return EXIT_FAILURE;
		}
	}
	if (closedir(proc_directory) == -1) {
		perror("closedir /proc");
		free(processes);
		return EXIT_FAILURE;
	}

	qsort(processes, count, sizeof(*processes), compare_processes);
	printf("Processes owned by %s (UID %lu):\n", user->pw_name,
		(unsigned long) user->pw_uid);
	printf("%8s  %s\n", "PID", "COMMAND");
	for (index = 0; index < count; ++index)
		printf("%8ld  %s\n", processes[index].pid,
			processes[index].command);
	printf("Total: %zu\n", count);

	free(processes);
	return EXIT_SUCCESS;
}

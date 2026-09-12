#define _POSIX_C_SOURCE 200809L

#include "fifo_io.h"

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_FIFO_PATH "temp.fifo"
#define MESSAGE_COUNT 10

enum program_mode {
	MODE_COMBINED,
	MODE_READER,
	MODE_WRITER
};

static void print_usage(const char *program_name)
{
	printf("Usage: %s [-r | -w] [-p FIFO_PATH]\n", program_name);
	printf("       %s -h\n", program_name);
	printf("  -r  collect and display messages\n");
	printf("  -w  write ten messages to a collector\n");
	printf("  -p  use FIFO_PATH instead of temp.fifo\n");
	printf("  -h  display this help\n");
}

static int run_combined(const char *requested_path)
{
	char temporary_directory[] = "/tmp/fifo_pipe.XXXXXX";
	char private_path[PATH_MAX];
	const char *path = requested_path;
	pid_t reader_pid;
	int reader_status;
	int writer_result;
	int uses_private_directory = requested_path == NULL;

	if (uses_private_directory) {
		if (mkdtemp(temporary_directory) == NULL) {
			perror("mkdtemp");
			return EXIT_FAILURE;
		}
		if (snprintf(private_path, sizeof(private_path), "%s/messages.fifo",
				temporary_directory) >= (int) sizeof(private_path)) {
			fprintf(stderr, "temporary FIFO path is too long\n");
			rmdir(temporary_directory);
			return EXIT_FAILURE;
		}
		path = private_path;
	}

	reader_pid = fork();
	if (reader_pid == -1) {
		perror("fork");
		if (uses_private_directory)
			rmdir(temporary_directory);
		return EXIT_FAILURE;
	}
	if (reader_pid == 0)
		return fifo_read_messages(path);

	writer_result = fifo_write_messages(path, MESSAGE_COUNT);
	if (kill(reader_pid, SIGTERM) == -1 && errno != ESRCH)
		perror("kill reader");
	if (waitpid(reader_pid, &reader_status, 0) == -1) {
		perror("waitpid");
		if (uses_private_directory)
			rmdir(temporary_directory);
		return EXIT_FAILURE;
	}

	if (uses_private_directory && rmdir(temporary_directory) == -1) {
		perror("rmdir temporary FIFO directory");
		return EXIT_FAILURE;
	}
	if (writer_result != EXIT_SUCCESS || !WIFEXITED(reader_status) ||
			WEXITSTATUS(reader_status) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	enum program_mode mode = MODE_COMBINED;
	const char *path = NULL;
	int option;
	int mode_selected = 0;

	while ((option = getopt(argc, argv, "rwp:h")) != -1) {
		switch (option) {
		case 'r':
			if (mode_selected) {
				fprintf(stderr, "choose only one of -r and -w\n");
				return EXIT_FAILURE;
			}
			mode = MODE_READER;
			mode_selected = 1;
			break;
		case 'w':
			if (mode_selected) {
				fprintf(stderr, "choose only one of -r and -w\n");
				return EXIT_FAILURE;
			}
			mode = MODE_WRITER;
			mode_selected = 1;
			break;
		case 'p':
			path = optarg;
			break;
		case 'h':
			print_usage(argv[0]);
			return EXIT_SUCCESS;
		default:
			print_usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (optind != argc) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}
	if (path != NULL && path[0] == '\0') {
		fprintf(stderr, "FIFO path must not be empty\n");
		return EXIT_FAILURE;
	}

	if (mode == MODE_READER)
		return fifo_read_messages(path != NULL ? path : DEFAULT_FIFO_PATH);
	if (mode == MODE_WRITER)
		return fifo_write_messages(path != NULL ? path : DEFAULT_FIFO_PATH,
			MESSAGE_COUNT);

	return run_combined(path);
}

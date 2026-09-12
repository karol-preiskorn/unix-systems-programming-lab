#define _POSIX_C_SOURCE 200809L

#include "fifo_io.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define FIFO_PERMS 0600
#define READ_BUFFER_SIZE 4096
#define WRITER_RETRIES 100

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
	(void) signal_number;
	stop_requested = 1;
}

static int install_reader_signal_handlers(void)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = request_stop;
	sigemptyset(&action.sa_mask);

	if (sigaction(SIGINT, &action, NULL) == -1 ||
			sigaction(SIGTERM, &action, NULL) == -1) {
		perror("sigaction");
		return -1;
	}

	return 0;
}

static int ignore_sigpipe(void)
{
	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = SIG_IGN;
	sigemptyset(&action.sa_mask);

	if (sigaction(SIGPIPE, &action, NULL) == -1) {
		perror("sigaction SIGPIPE");
		return -1;
	}

	return 0;
}

static int ensure_fifo(const char *path)
{
	struct stat status;

	if (mkfifo(path, FIFO_PERMS) == 0)
		return 0;

	if (errno != EEXIST) {
		perror("mkfifo");
		return -1;
	}
	if (lstat(path, &status) == -1) {
		perror("lstat FIFO");
		return -1;
	}
	if (!S_ISFIFO(status.st_mode)) {
		fprintf(stderr, "%s exists but is not a FIFO\n", path);
		return -1;
	}

	return 0;
}

static int verify_open_fifo(int fifo_fd, const char *path)
{
	struct stat status;

	if (fstat(fifo_fd, &status) == -1) {
		perror("fstat FIFO");
		return -1;
	}
	if (!S_ISFIFO(status.st_mode)) {
		fprintf(stderr, "%s is not a FIFO\n", path);
		return -1;
	}

	return 0;
}

static int open_fifo_writer(const char *path)
{
	const struct timespec retry_delay = { .tv_sec = 0, .tv_nsec = 20000000L };
	int fifo_fd;
	int attempt;

	for (attempt = 0; attempt < WRITER_RETRIES; ++attempt) {
		fifo_fd = open(path, O_WRONLY | O_NONBLOCK | O_NOFOLLOW);
		if (fifo_fd != -1) {
			if (verify_open_fifo(fifo_fd, path) == 0)
				return fifo_fd;
			close(fifo_fd);
			return -1;
		}
		if (errno != ENXIO && errno != ENOENT)
			break;
		nanosleep(&retry_delay, NULL);
	}

	perror("open FIFO for writing");
	return -1;
}

static int write_message(int fifo_fd, const char *message, size_t length)
{
	ssize_t bytes_written;

	do {
		bytes_written = write(fifo_fd, message, length);
	} while (bytes_written == -1 && errno == EINTR);

	if (bytes_written == -1) {
		perror("write FIFO");
		return -1;
	}
	if ((size_t) bytes_written != length) {
		fprintf(stderr, "incomplete FIFO write\n");
		return -1;
	}

	return 0;
}

int fifo_read_messages(const char *path)
{
	char buffer[READ_BUFFER_SIZE];
	ssize_t bytes_read;
	int fifo_fd;
	int descriptor_flags;
	int draining = 0;
	int result = EXIT_SUCCESS;

	stop_requested = 0;
	if (install_reader_signal_handlers() == -1 || ensure_fifo(path) == -1)
		return EXIT_FAILURE;

	fifo_fd = open(path, O_RDWR | O_NOFOLLOW);
	if (fifo_fd == -1) {
		perror("open FIFO for reading");
		return EXIT_FAILURE;
	}
	if (verify_open_fifo(fifo_fd, path) == -1) {
		close(fifo_fd);
		return EXIT_FAILURE;
	}
	if (flock(fifo_fd, LOCK_EX | LOCK_NB) == -1) {
		fprintf(stderr, "another collector already owns %s\n", path);
		close(fifo_fd);
		return EXIT_FAILURE;
	}

	for (;;) {
		if (stop_requested && !draining) {
			descriptor_flags = fcntl(fifo_fd, F_GETFL);
			if (descriptor_flags == -1 ||
					fcntl(fifo_fd, F_SETFL,
						descriptor_flags | O_NONBLOCK) == -1) {
				perror("set FIFO drain mode");
				result = EXIT_FAILURE;
				break;
			}
			draining = 1;
		}

		bytes_read = read(fifo_fd, buffer, sizeof(buffer));
		if (bytes_read > 0) {
			if (fwrite(buffer, 1, (size_t) bytes_read, stdout) !=
					(size_t) bytes_read) {
				perror("write stdout");
				result = EXIT_FAILURE;
				break;
			}
			fflush(stdout);
		} else if (bytes_read == -1 && errno == EINTR) {
			continue;
		} else if (draining && bytes_read == -1 &&
				errno == EAGAIN) {
			break;
		} else if (bytes_read == -1) {
			perror("read FIFO");
			result = EXIT_FAILURE;
			break;
		}
	}

	if (unlink(path) == -1 && errno != ENOENT) {
		perror("unlink FIFO");
		result = EXIT_FAILURE;
	}
	if (close(fifo_fd) == -1) {
		perror("close FIFO");
		result = EXIT_FAILURE;
	}

	return result;
}

int fifo_write_messages(const char *path, size_t message_count)
{
	char message[256];
	char timestamp[32];
	struct tm local_time;
	time_t current_time;
	size_t index;
	size_t message_length;
	int fifo_fd;
	int result = EXIT_SUCCESS;

	if (ignore_sigpipe() == -1 || ensure_fifo(path) == -1)
		return EXIT_FAILURE;
	fifo_fd = open_fifo_writer(path);
	if (fifo_fd == -1)
		return EXIT_FAILURE;

	for (index = 0; index < message_count; ++index) {
		current_time = time(NULL);
		if (localtime_r(&current_time, &local_time) == NULL ||
				strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
					&local_time) == 0) {
			fprintf(stderr, "failed to format current time\n");
			result = EXIT_FAILURE;
			break;
		}

		message_length = (size_t) snprintf(message, sizeof(message),
			"#%zu PID %ld: %s\n", index, (long) getpid(), timestamp);
		if (message_length >= sizeof(message) || message_length > PIPE_BUF) {
			fprintf(stderr, "FIFO message is too long\n");
			result = EXIT_FAILURE;
			break;
		}
		if (write_message(fifo_fd, message, message_length) == -1) {
			result = EXIT_FAILURE;
			break;
		}
	}

	if (close(fifo_fd) == -1) {
		perror("close FIFO");
		result = EXIT_FAILURE;
	}

	return result;
}

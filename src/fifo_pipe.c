#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FIFO_PATH "temp.fifo"
#define FIFO_PERMS 0666
#define MESSAGE_COUNT 10
#define READ_BUFFER_SIZE 4096
#define WRITER_RETRIES 100

static volatile sig_atomic_t stop_requested;

static void request_stop(int signal_number)
{
	(void) signal_number;
	stop_requested = 1;
}

static int install_signal_handlers(void)
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

static int ensure_fifo(void)
{
	struct stat status;

	if (mkfifo(FIFO_PATH, FIFO_PERMS) == 0)
		return 0;

	if (errno != EEXIST) {
		perror("mkfifo");
		return -1;
	}

	if (lstat(FIFO_PATH, &status) == -1) {
		perror("lstat");
		return -1;
	}

	if (!S_ISFIFO(status.st_mode)) {
		fprintf(stderr, "%s exists but is not a FIFO\n", FIFO_PATH);
		return -1;
	}

	return 0;
}

static int read_fifo(void)
{
	char buffer[READ_BUFFER_SIZE];
	ssize_t bytes_read;
	int fifo_fd;
	int result = EXIT_SUCCESS;

	if (ensure_fifo() == -1 || install_signal_handlers() == -1)
		return EXIT_FAILURE;

	/* O_RDWR keeps the collector alive while no external writer is connected. */
	fifo_fd = open(FIFO_PATH, O_RDWR);
	if (fifo_fd == -1) {
		perror("open FIFO for reading");
		unlink(FIFO_PATH);
		return EXIT_FAILURE;
	}

	while (!stop_requested) {
		bytes_read = read(fifo_fd, buffer, sizeof(buffer));
		if (bytes_read > 0) {
			if (fwrite(buffer, 1, (size_t) bytes_read, stdout) !=
					(size_t) bytes_read) {
				perror("write stdout");
				result = EXIT_FAILURE;
				break;
			}
			fflush(stdout);
		} else if (bytes_read == -1 && errno != EINTR) {
			perror("read FIFO");
			result = EXIT_FAILURE;
			break;
		}
	}

	if (close(fifo_fd) == -1) {
		perror("close FIFO");
		result = EXIT_FAILURE;
	}
	if (unlink(FIFO_PATH) == -1 && errno != ENOENT) {
		perror("unlink FIFO");
		result = EXIT_FAILURE;
	}

	return result;
}

static int open_fifo_writer(void)
{
	const struct timespec retry_delay = { .tv_sec = 0, .tv_nsec = 20000000L };
	int fifo_fd;
	int attempt;

	for (attempt = 0; attempt < WRITER_RETRIES; ++attempt) {
		fifo_fd = open(FIFO_PATH, O_WRONLY | O_NONBLOCK);
		if (fifo_fd != -1)
			return fifo_fd;
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

static int write_fifo(void)
{
	char message[256];
	char timestamp[32];
	struct tm local_time;
	time_t current_time;
	size_t message_length;
	int fifo_fd;
	int index;
	int result = EXIT_SUCCESS;

	if (ensure_fifo() == -1)
		return EXIT_FAILURE;

	fifo_fd = open_fifo_writer();
	if (fifo_fd == -1)
		return EXIT_FAILURE;

	for (index = 0; index < MESSAGE_COUNT; ++index) {
		current_time = time(NULL);
		if (localtime_r(&current_time, &local_time) == NULL ||
				strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S",
					&local_time) == 0) {
			fprintf(stderr, "failed to format current time\n");
			result = EXIT_FAILURE;
			break;
		}

		message_length = (size_t) snprintf(message, sizeof(message),
			"#%d PID %ld: %s\n", index, (long) getpid(), timestamp);
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

static void print_usage(const char *program_name)
{
	printf("Usage: %s [-r | -w | -h]\n", program_name);
	printf("  -r  collect and display messages from the FIFO\n");
	printf("  -w  write ten messages to an existing collector\n");
	printf("  -h  display this help\n");
}

int main(int argc, char *argv[])
{
	pid_t reader_pid;
	int option;
	int writer_result;
	int reader_status;

	while ((option = getopt(argc, argv, "rwh")) != -1) {
		switch (option) {
		case 'r':
			return read_fifo();
		case 'w':
			return write_fifo();
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

	reader_pid = fork();
	if (reader_pid == -1) {
		perror("fork");
		return EXIT_FAILURE;
	}
	if (reader_pid == 0)
		return read_fifo();

	writer_result = write_fifo();
	if (kill(reader_pid, SIGTERM) == -1 && errno != ESRCH)
		perror("kill reader");
	if (waitpid(reader_pid, &reader_status, 0) == -1) {
		perror("waitpid");
		return EXIT_FAILURE;
	}

	if (writer_result != EXIT_SUCCESS || !WIFEXITED(reader_status) ||
			WEXITSTATUS(reader_status) != EXIT_SUCCESS)
		return EXIT_FAILURE;

	return EXIT_SUCCESS;
}

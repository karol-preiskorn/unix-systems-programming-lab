#!/bin/sh

set -eu

CDPATH=''
export CDPATH
project_root=$(cd -- "$(dirname -- "$0")/.." && pwd)
temporary_directory=$(mktemp -d)
reader_pid=

cleanup()
{
	if [ -n "$reader_pid" ]; then
		kill "$reader_pid" 2>/dev/null || true
		wait "$reader_pid" 2>/dev/null || true
	fi
	rm -rf "$temporary_directory"
}

fail()
{
	printf 'FAIL: %s\n' "$1" >&2
	exit 1
}

assert_line_count()
{
	expected=$1
	file=$2
	actual=$(wc -l < "$file")
	[ "$actual" -eq "$expected" ] ||
		fail "expected $expected lines in $file, found $actual"
}

trap cleanup EXIT HUP INT TERM
cd "$project_root"

./bin/fifo_pipe > "$temporary_directory/combined.out"
assert_line_count 10 "$temporary_directory/combined.out"
grep -Eq '^#0 PID [0-9]+: [0-9]{4}-[0-9]{2}-[0-9]{2} ' \
	"$temporary_directory/combined.out" || fail "invalid FIFO record format"

fifo_path="$temporary_directory/messages.fifo"
./bin/fifo_pipe -r -p "$fifo_path" > "$temporary_directory/read.out" \
	2> "$temporary_directory/read.err" &
reader_pid=$!
attempt=0
while [ ! -p "$fifo_path" ] && [ "$attempt" -lt 100 ]; do
	sleep 0.02
	attempt=$((attempt + 1))
done
[ -p "$fifo_path" ] || fail "collector did not create the FIFO"

./bin/fifo_pipe -w -p "$fifo_path" &
writer_one=$!
./bin/fifo_pipe -w -p "$fifo_path" &
writer_two=$!
wait "$writer_one" || fail "first FIFO writer failed"
wait "$writer_two" || fail "second FIFO writer failed"

if ./bin/fifo_pipe -r -p "$fifo_path" \
		> "$temporary_directory/second-reader.out" \
		2> "$temporary_directory/second-reader.err"; then
	fail "a second FIFO collector was accepted"
fi
grep -q 'another collector already owns' \
	"$temporary_directory/second-reader.err" ||
	fail "second collector did not report ownership conflict"

kill -TERM "$reader_pid"
wait "$reader_pid" || fail "FIFO collector did not stop cleanly"
reader_pid=
assert_line_count 20 "$temporary_directory/read.out"
[ "$(sed -n 's/.* PID \([0-9][0-9]*\):.*/\1/p' \
	"$temporary_directory/read.out" | sort -u | wc -l)" -eq 2 ] ||
	fail "concurrent FIFO output did not contain two writer PIDs"
[ ! -e "$fifo_path" ] || fail "collector left the FIFO behind"

no_reader_path="$temporary_directory/no-reader.fifo"
if ./bin/fifo_pipe -w -p "$no_reader_path" \
		> "$temporary_directory/no-reader.out" \
		2> "$temporary_directory/no-reader.err"; then
	fail "writer succeeded without a collector"
fi

if ./bin/fifo_pipe -r -w > /dev/null 2>&1; then
	fail "conflicting FIFO modes were accepted"
fi

./bin/fork_env_z1 > "$temporary_directory/fork.out"
grep -q 'Child process' "$temporary_directory/fork.out" ||
	fail "fork example omitted child output"
grep -q 'global=2 automatic=101' "$temporary_directory/fork.out" ||
	fail "child values were not updated"
grep -q 'Parent after child process' "$temporary_directory/fork.out" ||
	fail "fork example omitted final parent output"
grep -q 'global=1 automatic=100' "$temporary_directory/fork.out" ||
	fail "parent values changed with the child"

./bin/example-1 > "$temporary_directory/example.out"
[ "$(grep -c '^parent process: counter=' "$temporary_directory/example.out")" \
	-eq 5 ] || fail "minimal example did not print five parent counters"
[ "$(grep -c '^child process: counter=' "$temporary_directory/example.out")" \
	-eq 5 ] || fail "minimal example did not print five child counters"
[ "$(grep -c '^--beginning of program$' "$temporary_directory/example.out")" \
	-eq 1 ] || fail "minimal example duplicated pre-fork output"

./bin/exec_inheritance > "$temporary_directory/exec.out"
grep -q 'EXEC_DEMO=preserved' "$temporary_directory/exec.out" ||
	fail "exec did not preserve the environment"
grep -Eq 'without FD_CLOEXEC: open$' "$temporary_directory/exec.out" ||
	fail "ordinary descriptor did not survive exec"
grep -Eq 'with FD_CLOEXEC: closed$' "$temporary_directory/exec.out" ||
	fail "FD_CLOEXEC descriptor survived exec"
grep -q 'ignored SIGUSR1 disposition: ignored' "$temporary_directory/exec.out" ||
	fail "ignored signal disposition did not survive exec"
grep -q 'caught SIGUSR2 disposition: default' "$temporary_directory/exec.out" ||
	fail "caught signal disposition did not reset during exec"

current_user=$(id -un)
./bin/proc_by_user "$current_user" > "$temporary_directory/proc.out"
grep -q "Processes owned by $current_user" "$temporary_directory/proc.out" ||
	fail "proc listing omitted its owner heading"
grep -Eq '^Total: [1-9][0-9]*$' "$temporary_directory/proc.out" ||
	fail "proc listing returned no processes"
if ./bin/proc_by_user __user_that_must_not_exist__ > /dev/null 2>&1; then
	fail "proc listing accepted an unknown user"
fi

printf 'All integration tests passed.\n'

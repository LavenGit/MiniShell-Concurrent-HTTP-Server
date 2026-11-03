CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2
all: minishell status_server
minishell: minishell.c
	$(CC) $(CFLAGS) -o $@ $<
status_server: status_server.c
	$(CC) $(CFLAGS) -pthread -o $@ $<
test: all
	python3 -m unittest discover -s tests -v
clean:
	rm -f minishell status_server
.PHONY: all test clean

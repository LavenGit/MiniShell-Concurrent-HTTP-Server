#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

#define MAX_TOKENS 256
#define MAX_COMMANDS 32
#define HISTORY_LIMIT 100

typedef struct { char *argv[MAX_TOKENS]; int argc; char *input; char *output; int append; } Command;
static char *history[HISTORY_LIMIT];
static int history_count;
static volatile sig_atomic_t foreground_pgid = 0;
typedef struct Job { pid_t pid; struct Job *next; } Job;
static Job *jobs = NULL;
static void track_job(pid_t pid) {
    Job *j = malloc(sizeof(*j));
    if (!j) { perror("malloc"); return; }
    j->pid = pid; j->next = jobs; jobs = j;
}
static void forget_job(pid_t pid) {
    Job **cur = &jobs;
    while (*cur) {
        if ((*cur)->pid == pid) { Job *old = *cur; *cur = old->next; free(old); return; }
        cur = &(*cur)->next;
    }
}
static void show_jobs(void) {
    for (Job *j = jobs; j; j = j->next) printf("[running] pid %ld\n", (long)j->pid);
}
static void clear_jobs(void) {
    while (jobs) { Job *old = jobs; jobs = jobs->next; free(old); }
}

static void on_sigint(int signo) { (void)signo; if (foreground_pgid > 0) kill(-foreground_pgid, SIGINT); else write(STDOUT_FILENO, "\n", 1); }
static void add_history(const char *line) {
    if (!*line) return;
    if (history_count == HISTORY_LIMIT) { free(history[0]); memmove(history, history + 1, (HISTORY_LIMIT - 1) * sizeof(*history)); history_count--; }
    history[history_count] = strdup(line);
    if (history[history_count]) history_count++;
}
static void show_history(void) { for (int i = 0; i < history_count; i++) printf("%d  %s\n", i + 1, history[i]); }
static int tokenize(char *line, char **tokens, int max) {
    int n = 0;
    char *p = line;
    while (*p) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (n >= max - 1) { fprintf(stderr, "too many tokens\n"); return -1; }
        if (strchr("|<>&", *p)) {
            char c = *p++;
            if (c == '>' && *p == '>') { p++; tokens[n++] = strdup(">>"); }
            else { char op[] = {c, '\0'}; tokens[n++] = strdup(op); }
        } else {
            char buf[8192]; size_t k = 0; char quote = 0;
            while (*p) {
                if (!quote && (isspace((unsigned char)*p) || strchr("|<>&", *p))) break;
                if (*p == '\\' && p[1]) { p++; if (k < sizeof(buf)-1) buf[k++] = *p++; continue; }
                if (*p == '\'' || *p == '"') {
                    if (!quote) { quote = *p++; continue; }
                    if (quote == *p) { quote = 0; p++; continue; }
                }
                if (k < sizeof(buf)-1) buf[k++] = *p;
                p++;
            }
            if (quote) { fprintf(stderr, "unclosed quote\n"); return -1; }
            buf[k] = '\0'; tokens[n++] = strdup(buf);
        }
        if (!tokens[n-1]) { perror("strdup"); return -1; }
    }
    tokens[n] = NULL;
    return n;
}
static int parse(char **t, int n, Command *cmds, int *count, int *background) {
    int c = 0;
    memset(cmds, 0, MAX_COMMANDS * sizeof(*cmds));
    *background = 0;
    for (int i = 0; i < n; i++) {
        if (!strcmp(t[i], "|")) {
            if (!cmds[c].argc || c + 1 >= MAX_COMMANDS) return -1;
            c++;
        } else if (!strcmp(t[i], "<") || !strcmp(t[i], ">") || !strcmp(t[i], ">>")) {
            if (i + 1 >= n || !strcmp(t[i+1], "|") || !strcmp(t[i+1], "&") || !strcmp(t[i+1], "<") || !strcmp(t[i+1], ">") || !strcmp(t[i+1], ">>")) return -1;
            if (!strcmp(t[i], "<")) cmds[c].input = t[++i];
            else { cmds[c].append = !strcmp(t[i], ">>"); cmds[c].output = t[++i]; }
        } else if (!strcmp(t[i], "&")) {
            if (i != n - 1) return -1;
            *background = 1;
        } else {
            if (cmds[c].argc >= MAX_TOKENS - 1) return -1;
            cmds[c].argv[cmds[c].argc++] = t[i];
        }
    }
    if (!cmds[c].argc) return -1;
    *count = c + 1;
    return 0;
}
static int redirect_files(const Command *cmd) {
    if (cmd->input) {
        int fd = open(cmd->input, O_RDONLY);
        if (fd < 0) { perror(cmd->input); return -1; }
        if (dup2(fd, STDIN_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
        close(fd);
    }
    if (cmd->output) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->output, flags, 0666);
        if (fd < 0) { perror(cmd->output); return -1; }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2"); close(fd); return -1; }
        close(fd);
    }
    return 0;
}
static int run_pipeline(Command *cmds, int count, int background) {
    pid_t pids[MAX_COMMANDS];
    pid_t pgid = 0;
    int prev_read = -1;
    int started = 0;
    for (int i = 0; i < count; i++) {
        int fd[2] = {-1, -1};
        if (i + 1 < count && pipe(fd) < 0) { perror("pipe"); break; }
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); if (fd[0]>=0) close(fd[0]); if (fd[1]>=0) close(fd[1]); break; }
        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            setpgid(0, pgid ? pgid : 0);
            if (prev_read >= 0) { if (dup2(prev_read, STDIN_FILENO) < 0) _exit(1); }
            if (fd[1] >= 0) { if (dup2(fd[1], STDOUT_FILENO) < 0) _exit(1); }
            if (prev_read >= 0) close(prev_read);
            if (fd[0] >= 0) close(fd[0]);
            if (fd[1] >= 0) close(fd[1]);
            if (redirect_files(&cmds[i]) < 0) _exit(1);
            execvp(cmds[i].argv[0], cmds[i].argv);
            perror(cmds[i].argv[0]);
            _exit(127);
        }
        if (!pgid) pgid = pid;
        setpgid(pid, pgid);
        pids[started++] = pid;
        if (prev_read >= 0) close(prev_read);
        if (fd[1] >= 0) close(fd[1]);
        prev_read = fd[0];
    }
    if (prev_read >= 0) close(prev_read);
    if (background) { if (started) { for (int i=0;i<started;i++) track_job(pids[i]); printf("[background] pid %ld\n", (long)pgid); } return 0; }
    foreground_pgid = pgid;
    int last_status = 0;
    for (int i = 0; i < started; i++) {
        int status;
        pid_t r;
        do { r = waitpid(pids[i], &status, 0); } while (r < 0 && errno == EINTR);
        if (r > 0 && i == started - 1) last_status = status;
    }
    foreground_pgid = 0;
    return WIFEXITED(last_status) ? WEXITSTATUS(last_status) : 1;
}
static void reap_background(void) { pid_t pid; while ((pid = waitpid(-1, NULL, WNOHANG)) > 0) forget_job(pid); }
int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    signal(SIGQUIT, SIG_IGN);
    char *line = NULL; size_t capacity = 0;
    int interactive = isatty(STDIN_FILENO);
    while (1) {
        reap_background();
        if (interactive) { printf("minishell$ "); fflush(stdout); }
        ssize_t len = getline(&line, &capacity, stdin);
        if (len < 0) { if (interactive) putchar('\n'); break; }
        if (len && line[len-1] == '\n') line[--len] = '\0';
        char *start = line; while (isspace((unsigned char)*start)) start++;
        if (!*start) continue;
        add_history(start);
        char *tokens[MAX_TOKENS] = {0};
        int n = tokenize(start, tokens, MAX_TOKENS);
        if (n > 0) {
            Command cmds[MAX_COMMANDS]; int count, background;
            if (parse(tokens, n, cmds, &count, &background) < 0) fprintf(stderr, "syntax error\n");
            else if (count == 1 && !background && !strcmp(cmds[0].argv[0], "exit")) {
                for (int i=0;i<n;i++) free(tokens[i]);
                break;
            } else if (count == 1 && !background && !strcmp(cmds[0].argv[0], "cd")) {
                const char *dir = cmds[0].argc > 1 ? cmds[0].argv[1] : getenv("HOME");
                if (!dir || chdir(dir) < 0) perror("cd");
            } else if (count == 1 && !background && !strcmp(cmds[0].argv[0], "history")) show_history();
            else if (count == 1 && !background && !strcmp(cmds[0].argv[0], "jobs")) { reap_background(); show_jobs(); }
            else run_pipeline(cmds, count, background);
        }
        for (int i = 0; i < MAX_TOKENS && tokens[i]; i++) free(tokens[i]);
    }
    free(line);
    clear_jobs();
    for (int i = 0; i < history_count; i++) free(history[i]);
    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

static char* trim(char *s) {
    while (*s && isspace((unsigned char)*s)) {
        s++;
    }
    if (*s == 0) {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        end--;
    }
    end[1] = 0;
    return s;
}

static char** split(const char *line, const char *delim, int *count) {
    *count = 0;
    char *copy = strdup(line);
    if (!copy) {
        return NULL;
    }

    char **parts = NULL;
    char *tok, *saveptr;

    for (tok = strtok_r(copy, delim, &saveptr); tok; tok = strtok_r(NULL, delim, &saveptr)) {
        char *clean = trim(tok);
        char *dup = strdup(clean);
        if (!dup) {
            for (int i = 0; i < *count; i++) {
                free(parts[i]);
            }
            free(parts);
            free(copy);
            return NULL;
        }
        char **new_parts = realloc(parts, (size_t)(*count + 2) * sizeof(char*));
        if (!new_parts) {
            free(dup);
            for (int i = 0; i < *count; i++) {
                free(parts[i]);
            }
            free(parts);
            free(copy);
            return NULL;
        }
        parts = new_parts;
        parts[*count] = dup;
        (*count)++;
    }

    if (parts) {
        parts[*count] = NULL;
    }
    free(copy);
    return parts;
}

static char** parse_argv(const char *s) {
    int argc = 0;
    char **argv = NULL;
    char *copy = strdup(s);
    if (!copy) {
        return NULL;
    }

    char *tok, *saveptr;
    for (tok = strtok_r(copy, " \t", &saveptr); tok; tok = strtok_r(NULL, " \t", &saveptr)) {
        char *dup = strdup(tok);
        if (!dup) {
            for (int i = 0; i < argc; i++) {
                free(argv[i]);
            }
            free(argv);
            free(copy);
            return NULL;
        }
        char **new_argv = realloc(argv, (size_t)(argc + 2) * sizeof(char*));
        if (!new_argv) {
            free(dup);
            for (int i = 0; i < argc; i++) {
                free(argv[i]);
            }
            free(argv);
            free(copy);
            return NULL;
        }
        argv = new_argv;
        argv[argc++] = dup;
    }

    if (argv) {
        argv[argc] = NULL;
    }
    free(copy);
    return argv;
}

static void free_argv(char **argv) {
    if (!argv) {
        return;
    }
    for (int i = 0; argv[i]; i++) {
        free(argv[i]);
    }
    free(argv);
}

static int run_pipeline(char ***argvs, int n) {
    if (n <= 0) {
        errno = EINVAL;
        return -1;
    }

    int (*pipes)[2] = NULL;
    if (n > 1) {
        pipes = malloc((size_t)(n - 1) * sizeof(*pipes));
        if (!pipes) {
            perror("malloc pipes");
            return -1;
        }
        for (int i = 0; i < n - 1; i++) {
            if (pipe(pipes[i]) == -1) {
                perror("pipe");
                // cerrar los ya abiertos
                for (int k = 0; k < i; k++) {
                    close(pipes[k][0]);
                    close(pipes[k][1]);
                }
                free(pipes);
                return -1;
            }
        }
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            if (pipes) {
                for (int j = 0; j < n - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                free(pipes);
            }
            while (waitpid(-1, NULL, WNOHANG) > 0) { /* no-op */ }
            return -1;
        }

        if (pid == 0) {
            if (n > 1) {
                if (i > 0) {
                    if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
                        perror("dup2 stdin");
                        _exit(1);
                    }
                }
                if (i < n - 1) {
                    if (dup2(pipes[i][1], STDOUT_FILENO) == -1) {
                        perror("dup2 stdout");
                        _exit(1);
                    }
                }
                for (int j = 0; j < n - 1; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }

            execvp(argvs[i][0], argvs[i]);
            perror("execvp");
            _exit(1);
        }
    }

    if (pipes) {
        for (int j = 0; j < n - 1; j++) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
        free(pipes);
    }
    for (int i = 0; i < n; i++) {
        int status;
        if (wait(&status) == -1) {
            perror("wait");
        }
    }

    return 0;
}

int main(void) {
    char *line = NULL;
    size_t cap = 0;

    for (;;) {
        printf("shell:$ ");
        fflush(stdout);

        ssize_t got = getline(&line, &cap, stdin);
        if (got == -1) {
            break;
        }

        char *cmdline = trim(line);
        if (*cmdline == '\0') {
            continue;
        }

        if (strcmp(cmdline, "exit") == 0) {
            break;
        }

        int nseg = 0;
        char **segs = split(cmdline, "|", &nseg);
        if (!segs || nseg == 0) {
            if (segs) {
                for (int i = 0; segs[i]; i++) {
                    free(segs[i]);
                }
                free(segs);
            }
            continue;
        }

        char ***argvs = malloc((size_t)nseg * sizeof(char**));
        if (!argvs) {
            perror("malloc argvs");
            for (int i = 0; segs[i]; i++) {
                free(segs[i]);
            }
            free(segs);
            continue;
        }

        int ok = 1;
        for (int i = 0; i < nseg; i++) {
            argvs[i] = parse_argv(segs[i]);
            if (!argvs[i] || !argvs[i][0]) {
                ok = 0;
                break;
            }
        }

        if (ok) {
            if (run_pipeline(argvs, nseg) != 0) {
                fprintf(stderr, "Error al ejecutar pipeline\n");
            }
        }

        for (int i = 0; i < nseg; i++) {
            free_argv(argvs[i]);
        }
        free(argvs);

        for (int i = 0; segs[i]; i++) {
            free(segs[i]);
        }
        free(segs);
    }

    free(line);
    return 0;
}

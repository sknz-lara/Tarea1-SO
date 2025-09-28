#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    if (*s == 0)
        return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    end[1] = 0;
    return s;
}

static char **split(const char *line, const char *delim, int *count)
{
    *count = 0;
    char *copy = strdup(line);
    if (!copy)
        return NULL;
    char **parts = NULL, *tok, *saveptr;
    for (tok = strtok_r(copy, delim, &saveptr); tok; tok = strtok_r(NULL, delim, &saveptr))
    {
        char *clean = trim(tok);
        char *dup = strdup(clean);
        if (!dup)
        {
            free(copy);
            for (int i = 0; i < *count; i++)
                free(parts[i]);
            free(parts);
            return NULL;
        }
        char **np = realloc(parts, (size_t)(*count + 2) * sizeof(char *));
        if (!np)
        {
            free(dup);
            free(copy);
            for (int i = 0; i < *count; i++)
                free(parts[i]);
            free(parts);
            return NULL;
        }
        parts = np;
        parts[*count] = dup;
        (*count)++;
    }
    if (parts)
        parts[*count] = NULL;
    free(copy);
    return parts;
}

static char **parse_argv(const char *s)
{
    int argc = 0;
    char **argv = NULL;
    char *copy = strdup(s);
    if (!copy)
        return NULL;
    char *tok, *saveptr;
    for (tok = strtok_r(copy, " \t\n\r", &saveptr); tok; tok = strtok_r(NULL, " \t\n\r", &saveptr))
    {
        char *dup = strdup(tok);
        if (!dup)
        {
            for (int i = 0; i < argc; i++)
                free(argv[i]);
            free(argv);
            free(copy);
            return NULL;
        }
        char **np = realloc(argv, (size_t)(argc + 2) * sizeof(char *));
        if (!np)
        {
            free(dup);
            for (int i = 0; i < argc; i++)
                free(argv[i]);
            free(argv);
            free(copy);
            return NULL;
        }
        argv = np;
        argv[argc++] = dup;
    }
    if (argv)
        argv[argc] = NULL;
    free(copy);
    return argv;
}
static void free_argv(char **argv)
{
    if (!argv)
        return;
    for (int i = 0; argv[i]; i++)
        free(argv[i]);
    free(argv);
}

static int run_pipeline(char ***argvs, int n)
{
    if (n <= 0)
    {
        errno = EINVAL;
        return -1;
    }
    int (*pipes)[2] = NULL;
    if (n > 1)
    {
        pipes = malloc((size_t)(n - 1) * sizeof(*pipes));
        if (!pipes)
        {
            perror("malloc pipes");
            return -1;
        }
        for (int i = 0; i < n - 1; i++)
        {
            if (pipe(pipes[i]) == -1)
            {
                perror("pipe");
                for (int k = 0; k < i; k++)
                {
                    close(pipes[k][0]);
                    close(pipes[k][1]);
                }
                free(pipes);
                return -1;
            }
        }
    }
    for (int i = 0; i < n; i++)
    {
        pid_t pid = fork();
        if (pid == -1)
        {
            perror("fork");
            if (pipes)
            {
                for (int j = 0; j < n - 1; j++)
                {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
                free(pipes);
            }
            while (wait(NULL) > 0)
            {
            }
            return -1;
        }
        if (pid == 0)
        {
            if (n > 1)
            {
                if (i > 0)
                {
                    if (dup2(pipes[i - 1][0], STDIN_FILENO) == -1)
                    {
                        perror("dup2 stdin");
                        _exit(1);
                    }
                }
                if (i < n - 1)
                {
                    if (dup2(pipes[i][1], STDOUT_FILENO) == -1)
                    {
                        perror("dup2 stdout");
                        _exit(1);
                    }
                }
                for (int j = 0; j < n - 1; j++)
                {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }
            execvp(argvs[i][0], argvs[i]);
            perror("execvp");
            _exit(127);
        }
    }
    if (pipes)
    {
        for (int j = 0; j < n - 1; j++)
        {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
        free(pipes);
    }
    for (int i = 0; i < n; i++)
        wait(NULL);
    return 0;
}

static volatile sig_atomic_t g_timed_out = 0;
static volatile sig_atomic_t g_child_pgid = 0;

static void on_alarm(int sig)
{
    (void)sig;
    if (g_child_pgid > 0)
    {
        kill(-g_child_pgid, SIGKILL);
    }
    g_timed_out = 1;
}

static char *join_tokens(char **argv, int start_idx)
{
    size_t len = 0;
    for (int i = start_idx; argv[i]; i++)
        len += strlen(argv[i]) + 1;
    if (len == 0)
        return NULL;
    char *buf = malloc(len + 1);
    if (!buf)
        return NULL;
    buf[0] = '\0';
    for (int i = start_idx; argv[i]; i++)
    {
        strcat(buf, argv[i]);
        if (argv[i + 1])
            strcat(buf, " ");
    }
    return buf;
}

static void print_and_optionally_save(const char *cmdline,
                                      double real_time,
                                      double user_time,
                                      double sys_time,
                                      long maxrss_kb,
                                      const char *savefile)
{
    printf("\n--- Mediciones de miprof ---\n");
    printf("Comando: %s\n", cmdline);
    printf("Tiempo real: %.6f s\n", real_time);
    printf("Tiempo de usuario: %.6f s\n", user_time);
    printf("Tiempo de sistema: %.6f s\n", sys_time);
    printf("Pico de memoria residente: %ld KB\n", maxrss_kb);
    printf("---------------------------\n\n");

    if (savefile)
    {
        int fd = open(savefile, O_CREAT | O_APPEND | O_WRONLY, 0644);
        if (fd == -1)
        {
            perror("open (ejecsave)");
            return;
        }
        dprintf(fd,
                "Comando: %s\nTiempo real: %.6f s\nTiempo usuario: %.6f s\nTiempo sistema: %.6f s\nMaxRSS: %ld KB\n---\n",
                cmdline, real_time, user_time, sys_time, maxrss_kb);
        close(fd);
        printf("[Resultados guardados en %s]\n\n", savefile);
    }
}
static int run_and_measure_cmdline(const char *cmdline, const char *savefile, int timeout_secs)
{
    if (!cmdline || !*cmdline)
    {
        fprintf(stderr, "miprof: comando vacío\n");
        return -1;
    }

    struct timeval real_start, real_end;
    struct rusage usage;
    gettimeofday(&real_start, NULL);

    struct sigaction sa = {0};
    if (timeout_secs > 0)
    {
        sa.sa_handler = on_alarm;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        if (sigaction(SIGALRM, &sa, NULL) == -1)
        {
            perror("sigaction");
            return -1;
        }
    }

    g_timed_out = 0;
    g_child_pgid = 0;

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        setpgid(0, 0);
        execl("/bin/sh", "sh", "-c", cmdline, (char *)NULL);
        fprintf(stderr, "Error al ejecutar comando con sh -c: %s\n", strerror(errno));
        _exit(127);
    }

    setpgid(pid, pid);
    g_child_pgid = pid;

    if (timeout_secs > 0)
    {
        struct itimerval it = {0};
        it.it_value.tv_sec = timeout_secs;
        if (setitimer(ITIMER_REAL, &it, NULL) == -1)
        {
            perror("setitimer");
        }
    }

    int status;
    if (wait4(pid, &status, 0, &usage) == -1)
    {
        if (errno != EINTR)
        {
            perror("wait4");
        }
        while (waitpid(pid, &status, 0) == -1 && errno == EINTR)
        {
        }
    }
    if (timeout_secs > 0)
    {
        struct itimerval zero = {0};
        setitimer(ITIMER_REAL, &zero, NULL);
    }

    gettimeofday(&real_end, NULL);

    double real_time = (real_end.tv_sec - real_start.tv_sec) + (real_end.tv_usec - real_start.tv_usec) / 1000000.0;
    double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
    double sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
    long maxrss_kb = usage.ru_maxrss;

    if (g_timed_out)
    {
        printf("\n[miprof] El proceso excedió el tiempo máximo (%d s) y fue terminado.\n", timeout_secs);
    }

    print_and_optionally_save(cmdline, real_time, user_time, sys_time, maxrss_kb, savefile);
    return 0;
}

/* =====================  MAIN SHELL  ===================== */

int main(void)
{
    char *line = NULL;
    size_t cap = 0;

    for (;;)
    {
        printf("mishell> ");
        fflush(stdout);

        if (getline(&line, &cap, stdin) == -1)
        {
            printf("\n");
            break;
        }

        char *cmdline = trim(line);
        if (*cmdline == '\0')
            continue;
        if (strcmp(cmdline, "exit") == 0)
            break;

        char **first_args = parse_argv(cmdline);
        if (!first_args || !first_args[0])
        {
            free_argv(first_args);
            continue;
        }

        if (strcmp(first_args[0], "miprof") == 0)
        {
            const char *arg1 = first_args[1];

            // caso 1: "ejecsave"
            if (arg1 && strcmp(arg1, "ejecsave") == 0)
            {
                const char *savefile = first_args[2];
                char *cmd = join_tokens(first_args, 3);
                if (!savefile || !cmd)
                {
                    fprintf(stderr, "Uso: miprof ejecsave <archivo> <comando...>\n");
                }
                else
                {
                    run_and_measure_cmdline(cmd, savefile, 0);
                }
                free(cmd);

                // caso 2: "ejecutar" con limite de tiempo
            }
            else if (arg1 && strcmp(arg1, "ejecutar") == 0)
            {
                const char *secs_str = first_args[2];
                char *cmd = join_tokens(first_args, 3);
                int secs = secs_str ? atoi(secs_str) : 0;
                if (secs <= 0 || !cmd)
                {
                    fprintf(stderr, "Uso: miprof ejecutar <segundos> <comando...>\n");
                }
                else
                {
                    run_and_measure_cmdline(cmd, NULL, secs);
                }
                free(cmd);

                // caso 3: por defecto ("ejec" opcional o sin nada)
            }
            else
            {
                int start_index = 1;
                // si el primer argumento es "ejec", el comando empieza en el siguiente.
                if (arg1 && strcmp(arg1, "ejec") == 0)
                {
                    start_index = 2;
                }

                char *cmd = join_tokens(first_args, start_index);
                if (!cmd)
                {
                    // si no hay comando
                    fprintf(stderr, "Uso: miprof [ejec|ejecsave archivo|ejecutar <segundos>] comando args...\n");
                }
                else
                {
                    run_and_measure_cmdline(cmd, NULL, 0);
                }
                free(cmd);
            }

            free_argv(first_args);
            continue; // salta al siguiente ciclo
        }
        free_argv(first_args);
        int nseg = 0;
        char **segs = split(cmdline, "|", &nseg);
        if (!segs)
            continue;
        char ***argvs = malloc((size_t)nseg * sizeof(char **));
        if (!argvs)
        {
            perror("malloc");
            for (int i = 0; segs[i]; i++)
                free(segs[i]);
            free(segs);
            continue;
        }

        int ok = 1;
        for (int i = 0; i < nseg; i++)
        {
            argvs[i] = parse_argv(segs[i]);
            if (!argvs[i] || !argvs[i][0])
            {
                ok = 0;
                break;
            }
        }
        if (ok)
            run_pipeline(argvs, nseg);

        for (int i = 0; i < nseg; i++)
            free_argv(argvs[i]);
        free(argvs);
        for (int i = 0; segs[i]; i++)
            free(segs[i]);
        free(segs);
    }

    free(line);
    return 0;
}

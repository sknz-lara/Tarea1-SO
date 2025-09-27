#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>     // para gettimeofday
#include <sys/resource.h> // para getrusage y wait4

static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
    {
        s++;
    }
    if (*s == 0)
    {
        return s;
    }
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
    {
        end--;
    }
    end[1] = 0;
    return s;
}

static char **split(const char *line, const char *delim, int *count)
{
    *count = 0;
    char *copy = strdup(line);
    if (!copy)
        return NULL;

    char **parts = NULL;
    char *tok, *saveptr;

    for (tok = strtok_r(copy, delim, &saveptr); tok; tok = strtok_r(NULL, delim, &saveptr))
    {
        char *clean = trim(tok);
        char *dup = strdup(clean);
        if (!dup)
        {
            for (int i = 0; i < *count; i++)
                free(parts[i]);
            free(parts);
            free(copy);
            return NULL;
        }
        char **new_parts = realloc(parts, (size_t)(*count + 2) * sizeof(char *));
        if (!new_parts)
        {
            free(dup);
            for (int i = 0; i < *count; i++)
                free(parts[i]);
            free(parts);
            free(copy);
            return NULL;
        }
        parts = new_parts;
        parts[*count] = dup;
        (*count)++;
    }

    if (parts)
    {
        parts[*count] = NULL;
    }
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
        char **new_argv = realloc(argv, (size_t)(argc + 2) * sizeof(char *));
        if (!new_argv)
        {
            free(dup);
            for (int i = 0; i < argc; i++)
                free(argv[i]);
            free(argv);
            free(copy);
            return NULL;
        }
        argv = new_argv;
        argv[argc++] = dup;
    }

    if (argv)
    {
        argv[argc] = NULL;
    }
    free(copy);
    return argv;
}

static void free_argv(char **argv)
{
    if (!argv)
        return;
    for (int i = 0; argv[i]; i++)
    {
        free(argv[i]);
    }
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
                ; // Limpia procesos hijos ya creados
            return -1;
        }

        if (pid == 0)
        { // Proceso hijo
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
    {
        wait(NULL);
    }
    return 0;
}

/**
 * @brief Ejecuta un comando, mide su tiempo y uso de recursos.
 */
static int run_and_measure(char **argv)
{
    if (!argv || !argv[0])
    {
        fprintf(stderr, "miprof: No se reconoce ningun comando para ejecutar.\n");
        return 0;
    }

    struct timeval real_start, real_end;
    struct rusage usage;

    // toma el tiempo real de inicio, justo antes de la ejecucion
    gettimeofday(&real_start, NULL);

    pid_t pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return -1;
    }

    if (pid == 0)
    {
        // Proceso hijo: ejecuta el comando
        execvp(argv[0], argv);
        // Si execvp retorna, es porque hubo un error
        fprintf(stderr, "Error al ejecutar '%s': %s\n", argv[0], strerror(errno));
        _exit(127); // salida estandar para "comando no encontrado"
    }
    else
    {
        // padre espera al hijo y mide los recursos
        int status;
        if (wait4(pid, &status, 0, &usage) == -1) // wait4 para el tiempo de CPU y la memoria
        {
            perror("wait4");
            return -1;
        }

        // tiempo real de finalización
        gettimeofday(&real_end, NULL);

        // tiempo real
        double real_time = (real_end.tv_sec - real_start.tv_sec) +
                           (real_end.tv_usec - real_start.tv_usec) / 1000000.0;

        // tiempo de usuario (CPU time en modo usuario)
        double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;

        // tiempo del sistema (CPU time en modo kernel)
        double sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;

        // max de memoria
        long maxrss_kb = usage.ru_maxrss;

        printf("\n--- Mediciones de miprof ---\n");
        printf("Comando: %s\n", argv[0]);
        printf("Tiempo real: %.6f s\n", real_time);
        printf("Tiempo de usuario: %.6f s\n", user_time);
        printf("Tiempo de sistema: %.6f s\n", sys_time);
        printf("Pico de memoria residente: %ld KB\n", maxrss_kb);
        printf("---------------------------\n\n");
    }

    return 0;
}

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
        {
            continue;
        }
        if (strcmp(cmdline, "exit") == 0)
        {
            break;
        }

        // revisa si el comando 'miprof' esta de buscar pipes
        char **first_args = parse_argv(cmdline);
        if (!first_args || !first_args[0])
        {
            free_argv(first_args);
            continue;
        }

        if (strcmp(first_args[0], "miprof") == 0)
        {
            // ej: "miprof ls -l", se debe ejecutar "ls -l"
            // first_args + 1 apunta al segundo elemento ("ls")
            run_and_measure(first_args + 1);
            free_argv(first_args); // libera la memoria del parseo
        }
        else
        {// si no es miprof, se procesa como un comando simple o con pipelines.
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
            {
                run_pipeline(argvs, nseg);
            }

            // Se libera la memoria
            for (int i = 0; i < nseg; i++)
                free_argv(argvs[i]);
            free(argvs);
            for (int i = 0; segs[i]; i++)
                free(segs[i]);
            free(segs);
        }
    }

    free(line);
    return 0;
}

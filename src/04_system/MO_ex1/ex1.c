/**
 *
 * Project: HEIA-FR / HES-SO MSE - MA-CSEL1 Laboratory
 *
 * Abstract: Multiprocessing et Ordonnanceur
 *
 * Purpose: Processus, signaux et communication
 *
 * Autĥor:  Nicolas.rivier
 * Date:    02.06.2026
 *
 *
 * Exercice #1: Concevez et développez une petite application mettant en œuvre
 * un des services de communication proposés par Linux (par exemple socketpair)
 * entre un processus parent et un processus enfant. Le processus enfant devra
 * émettre quelques messages sous forme de texte vers le processus parent,
 * lequel les affichera sur la console. Le message exit permettra de terminer
 * l’application. Cette application devra impérativement capturer les signaux
 * SIGHUP, SIGINT, SIGQUIT, SIGABRT et SIGTERM et les ignorer. Seul un message
 * d’information sera affiché sur la console. Chacun des processus devra
 * utiliser son propre cœur, par exemple core 0 pour le parent, et core 1 pour
 * l’enfant.
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

void catch_signal(int signo) { printf("Signal [%d] arrived: \n", signo); }

void get_time(char* buf, size_t len) {
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(buf, len, "Hi there, it's time %H:%M:%S", &tm_now);
}

void parents(int fd) {
    char buf[50];
    int ret;

    while (1) {
        ret = read(fd, buf, sizeof(buf));
        if (ret == -1) {
            if ((errno == EINTR)) {
                printf("P: Failed read socket EINT arrive, continue...\n");
                continue;
            }
            err(EXIT_FAILURE, "Failed read socket");
        } else if (ret > 0) {
            if (strcmp(buf, "exit") == 0) {
                printf("P: Receve EXIT from child\n");
                break;
            }
            printf("P: msg from child: %s\n", buf);
        }
    }

    int status = 0;
    pid_t pid = waitpid(-1, &status, 0);
    printf("P: Child [%d] exit with status: %d\n", pid, status);
}

void child(int fd) {
    char buf[50];

    for (int i = 0; i < 100; i++) {
        get_time(buf, sizeof(buf));
        sprintf(buf, "(%d):%s", i, buf);
        write(fd, buf, sizeof(buf));
        sleep(1);
    }

    write(fd, "exit", sizeof(char) * 5);
    sleep(2);
    exit(0);
}

void set_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    int ret = sched_setaffinity(0, sizeof(set), &set);
    if (ret == -1) {
        err(EXIT_FAILURE, "Failed change child cpu");
    }
}

int main(int argc, char* argv[]) {
    struct sigaction act = {
        .sa_handler = catch_signal,
    };
    int ret = sigaction(SIGHUP, &act, NULL);
    if (ret == -1) err(EXIT_FAILURE, "SIGHUP not redircet");
    ret = sigaction(SIGINT, &act, NULL);
    if (ret == -1) err(EXIT_FAILURE, "SIGINT not redircet");
    ret = sigaction(SIGQUIT, &act, NULL);
    if (ret == -1) err(EXIT_FAILURE, "SIGQUIT not redircet");
    ret = sigaction(SIGABRT, &act, NULL);
    if (ret == -1) err(EXIT_FAILURE, "SIGABRT not redircet");
    ret = sigaction(SIGTERM, &act, NULL);
    if (ret == -1) err(EXIT_FAILURE, "SIGTERM not redircet");

    int fd[2];
    ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if (ret == -1) {
        err(EXIT_FAILURE, "Failed creat Socket");
    }

    pid_t pid = fork();
    if (pid == 0) {
        /* code de l'enfant */
        printf("C: Child id [%d]\n", getpid());
        set_cpu(1);
        child(fd[1]);
    } else if (pid > 0) {
        /* code du parent */
        printf("P: Parents is [%d] creat Child [%d]\n", getpid(), pid);
        set_cpu(0);
        parents(fd[0]);
    } else {
        err(EXIT_FAILURE, "Failed fork");
    }

    return 0;
}
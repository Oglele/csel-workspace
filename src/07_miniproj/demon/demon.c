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
 */
#define _GNU_SOURCE

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
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
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <sys/epoll.h>


#include "ssd1306.h"

#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_LED "/sys/class/gpio/gpio362"

#define GPIO_BTN_A0 "/sys/class/gpio/gpio0"
#define GPIO_BTN_A2 "/sys/class/gpio/gpio2"
#define GPIO_BTN_A3 "/sys/class/gpio/gpio3"

#define LED "362"
#define BTN_A0 "0"
#define BTN_A2 "2"
#define BTN_A3 "3"

#define MAX_EVENTS 3

static int open_led() {
    // unexport pin out of sysfs (reinitialization)
    int f = open(GPIO_UNEXPORT, O_WRONLY);

    write(f, LED, strlen(LED));
    close(f);

    // export pin to sysfs
    f = open(GPIO_EXPORT, O_WRONLY);
    write(f, LED, strlen(LED));
    close(f);

    // config pin
    f = open(GPIO_LED "/direction", O_WRONLY);
    write(f, "out", 3);
    close(f);

    // open gpio value attribute
    f = open(GPIO_LED "/value", O_RDWR);
    return f;
}

static int open_bnt(char* num_port) {
    char path[64];
    // unexport pin out of sysfs (reinitialization)
    int f = open(GPIO_UNEXPORT, O_WRONLY);

    write(f, num_port, strlen(num_port));
    close(f);

    // export pin to sysfs
    f = open(GPIO_EXPORT, O_WRONLY);
    write(f, num_port, strlen(num_port));
    close(f);

    // config pin
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/direction", num_port);
    f = open(path, O_WRONLY);
    write(f, "in", 2);
    close(f);

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/edge", num_port);
    f = open(path, O_WRONLY);
    write(f, "rising", 6);
    close(f);

    // open gpio value attribute
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%s/value", num_port);
    f = open(path, O_RDWR);
    return f;
}

void catch_signal(int signo) { printf("Signal [%d] arrived: \n", signo); }

void demon(void) {
    int ret;

    ssd1306_init();
    ssd1306_set_position(0, 0);
    ssd1306_puts("CSEL RIVIER :07");
    ssd1306_set_position(0, 1);
    ssd1306_puts("Demon      v0.1");
    ssd1306_set_position(0, 2);
    ssd1306_puts("--------------");

    int fd_led = open_led();
    int fd_btnA0 = open_bnt(BTN_A0);
    int fd_btnA2 = open_bnt(BTN_A2);
    int fd_btnA3 = open_bnt(BTN_A3);


    int fd_ep = epoll_create1(0);
    if (fd_ep == -1)
        err(EXIT_FAILURE, "Failed creat epoll");

    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN | EPOLLPRI | EPOLLET;
    ev.data.fd = fd_btnA0;

    ret = epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd_btnA0, &ev);
    if (ret == -1)
        err(EXIT_FAILURE, "Failed config epoll");

    ev.events = EPOLLIN | EPOLLPRI | EPOLLET;
    ev.data.fd = fd_btnA2;

    ret = epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd_btnA2, &ev);
    if (ret == -1)
        err(EXIT_FAILURE, "Failed config epoll");

    ev.events = EPOLLIN | EPOLLPRI | EPOLLET;
    ev.data.fd = fd_btnA3;

    ret = epoll_ctl(fd_ep, EPOLL_CTL_ADD, fd_btnA3, &ev);
    if (ret == -1)
        err(EXIT_FAILURE, "Failed config epoll");


    while (1) {
        int n = epoll_wait(fd_ep, events, MAX_EVENTS, -1);
        if (n == -1){
            if (errno == EINTR) continue;
            err(EXIT_FAILURE, "Failed receive events");
        }

        for (int i = 0; i < n; i++)
        {
            char buf[2];
            int current_fd = events[i].data.fd;
            syslog(LOG_INFO, "event=%ld on fd=%d\n", events[i].events, events[i].data.fd);

        }
        
    }
}



int main(int argc, char* argv[]) {
    int fd[2];
    int ret = socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if (ret == -1) {
        err(EXIT_FAILURE, "Failed creat Socket");
    }

    // 1. Créer un nouveau processus et terminer le processus parent
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS);

    // 2. Créer une nouvelle session pour le nouveau processus
    if (setsid() < 0) exit(EXIT_FAILURE);

    // 3. Second fork Ccréer le processus démon et terminer le processus parent
    pid_t pid_d = fork();
    if (pid_d < 0) exit(EXIT_FAILURE);
    if (pid_d > 0) exit(EXIT_SUCCESS);

    // 4. Capturer les signaux souhaités
    struct sigaction act = {
        .sa_handler = catch_signal,
    };
    ret = sigaction(SIGHUP, &act, NULL);
    if (ret == -1) err(EXIT_FAILURE, "SIGHUP not redircet");
    ret = sigaction(SIGINT, &act, NULL);
    if (ret == -1) err(EXIT_FAILURE, "SIGINT not redircet");
    ret = sigaction(SIGQUIT, &act, NULL);

    // 5. Mettre à jour le masque pour la création de fichiers
    umask(0);

    // 6. Mettre à jour le masque pour la création de fichiers
    if (chdir("/") < 0) exit(EXIT_FAILURE);

    // 7. Fermer tous les descripteurs de fichiers:
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    // 8. Rediriger stdin, stdout et stderr vers /dev/null
    open("/dev/null", O_RDONLY);  // stdin
    open("/dev/null", O_WRONLY);  // stdout
    open("/dev/null", O_RDWR);    // stderr

    // 9. Option: ouvrir un fichier de logging, par exemple sous syslog:
    openlog("MyDemon", LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Le démon id [%d] a démarré avec succès.", getpid());

    demon();

    closelog();
    return 0;
}
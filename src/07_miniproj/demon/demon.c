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
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "commun.h"
#include "ssd1306.h"

#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_LED "/sys/class/gpio/gpio362"

#define GPIO_BTN_A0 "/sys/class/gpio/gpio0"
#define GPIO_BTN_A2 "/sys/class/gpio/gpio2"
#define GPIO_BTN_A3 "/sys/class/gpio/gpio3"

#define SOCKET_PATH "/tmp/demon.sock"

#define LED "362"
#define BTN_A0 "0"
#define BTN_A2 "2"
#define BTN_A3 "3"

#define MAX_EVENTS 6

static module_config_t config = {.mode = MODE_AUTO, .frequency = 5, .duty = 50};

typedef enum { BTN_UP, BTN_DOWN, BTN_MODE } btn_id_t;

typedef void (*event_handler_t)(int fd, void* user_data);

typedef struct {
    int fd;
    int fd_epoll;
    uint32_t events_mask;
    event_handler_t handler;
    char name[32];
    btn_id_t btn_id;
} EventContext;

static void handle_button(int fd, EventContext* ctx);
static void handle_socket_client(int fd, EventContext* ctx);
static void handle_socket_server(int fd, EventContext* ctx);
static void handle_timer(int fd, EventContext* ctx);

static void register_event(int epoll_fd, EventContext* ctx, int fd);
int create_unix_socket(const char* path);
static int open_led();
static int open_bnt(char* num_port);
static void sysfs_write_conf(const char* path, module_config_t conf);
static void sysfs_read_temp(const char* path, int * temp);

static EventContext ctx_btns[] = {{.events_mask = EPOLLIN | EPOLLET | EPOLLPRI,
                                   .handler = handle_button,
                                   .name = "btnA0",
                                   .btn_id = BTN_UP},
                                  {.events_mask = EPOLLIN | EPOLLET | EPOLLPRI,
                                   .handler = handle_button,
                                   .name = "btnA2",
                                   .btn_id = BTN_DOWN},
                                  {.events_mask = EPOLLIN | EPOLLET | EPOLLPRI,
                                   .handler = handle_button,
                                   .name = "btnA3",
                                   .btn_id = BTN_MODE}};

static EventContext ctx_socket_cli = {.events_mask = EPOLLIN | EPOLLET,
                                      .handler = handle_socket_client,
                                      .name = "socket_client"};

static EventContext ctx_socket_srv = {.events_mask = EPOLLIN,
                                      .handler = handle_socket_server,
                                      .name = "Socket Serveur"};

static EventContext ctx_timer = {
    .events_mask = EPOLLIN, .handler = handle_timer, .name = "timer"};


void update_display(){

    int temp;
    sysfs_read_temp(module_temp_path,&temp);

    char tmp_buf[16];
    int len = snprintf(tmp_buf, sizeof(tmp_buf), "%s %d %d  ",
                       str_mode_option[config.mode], config.frequency, config.duty);

    if (len < 0 || len >= sizeof(tmp_buf)) {
        syslog(LOG_ERR,
               "Erreur : Données de configuration trop longues pour le buffer");
        return;
    }

    ssd1306_set_position(1, 3);
    ssd1306_puts(tmp_buf);

    len = snprintf(tmp_buf, sizeof(tmp_buf), "tmp: %d,%02d C",temp/1000,temp%1000);

    if (len < 0 || len >= sizeof(tmp_buf)) {
        syslog(LOG_ERR,
               "Erreur : Données de configuration trop longues pour le buffer");
        return;
    }

    ssd1306_set_position(1, 4);
    ssd1306_puts(tmp_buf);

}

void handle_timer(int fd, EventContext* ctx) {
    uint64_t expirations;
    if (read(fd, &expirations, sizeof(expirations)) > 0) {
        syslog(LOG_INFO, "Timer expiré : mise à jour OLED");
        update_display();
    }
}

void handle_button(int fd, EventContext* ctx) {
    char dummy[2];
    lseek(fd, 0, SEEK_SET);
    read(fd, &dummy, 1);
    switch (ctx->btn_id) {
        case BTN_UP:
            syslog(LOG_INFO, "Name: %s UP on fd=%d ctx-fd%d\n", ctx->name, fd,
                   ctx->fd);
            if (config.mode == MODE_MANUAL) {
                config.frequency =
                    clamp(config.frequency + 1, FREQ_MIN, FREQ_MAX);
                sysfs_write_conf(module_conf_path, config);
            }

            break;

        case BTN_DOWN:
            if (config.mode == MODE_MANUAL) {
                config.frequency =
                    clamp(config.frequency - 1, FREQ_MIN, FREQ_MAX);
                sysfs_write_conf(module_conf_path, config);
            }
            syslog(LOG_INFO, "Name: %s DOWN on fd=%d ctx-fd%d\n", ctx->name, fd,
                   ctx->fd);
            break;

        case BTN_MODE:

            config.mode = (config.mode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
            syslog(LOG_INFO, "Name: %s MODE on fd=%d ctx-fd%d\n", ctx->name, fd,
                   ctx->fd);
            sysfs_write_conf(module_conf_path, config);
            break;

        default:
            return;  // On sort si l'ID est inconnu
    }
    update_display();
}

int match_string(const char *const *array, const char *string) {
    for (int i = 0; array[i] != NULL; i++) {
        if (strcmp(array[i], string) == 0) {
            return i; // Retourne l'index (0 pour auto, 1 pour manual)
        }
    }
    return -1; // Non trouvé
}

void handle_socket_client(int fd, EventContext* ctx) {
    // EventContext* ctx = (EventContext*)user_data;

    char msg[128];
    int bytes = read(fd, msg, sizeof(msg) - 1);
    if (bytes <= 0) {
        // Le client s'est déconnecté
        epoll_ctl(ctx->fd_epoll, EPOLL_CTL_DEL, fd, NULL);
        syslog(LOG_INFO, "socket client disconnect fd %d\n", fd);
        close(fd);
    } else {
        msg[bytes] = '\0';  // Fin de chaîne
        syslog(LOG_INFO, "socket receive fd %d: %s", fd, msg);
    }

    char tmp_buf[25];
    int ret = sscanf(msg, "%9s %d %d", tmp_buf, &config.frequency, &config.duty);
    if (ret != 3) {
        return -EINVAL;
    }
    int idx = match_string(str_mode_option, tmp_buf);
    if (idx < 0){
        syslog(LOG_INFO, "Mode invalide : 'auto' ou 'manual' uniquement\n");
        return -EINVAL;
    }
    config.mode = (module_mode_t)idx;
}

void handle_socket_server(int fd, EventContext* ctx) {
    // EventContext* ctx = (EventContext*)user_data;
    struct epoll_event ev;

    int fd_cli = accept(fd, NULL, NULL);

    register_event(ctx->fd_epoll, &ctx_socket_cli, fd_cli);

    syslog(LOG_INFO, "Socket accept client fd %d", fd_cli);
}

void register_event(int epoll_fd, EventContext* ctx, int fd) {
    struct epoll_event ev;

    ctx->fd = fd;
    ctx->fd_epoll = epoll_fd;
    ev.events = ctx->events_mask;
    ev.data.ptr = ctx;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        syslog(LOG_ERR, "Failed config epoll: %s", ctx->name);
        exit(EXIT_FAILURE);
    }
    syslog(LOG_INFO, "epoll register: fd %d\n", fd);
}

int create_unix_socket(const char* path) {
    int fd;
    struct sockaddr_un addr;

    fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed creat socket");
        exit(EXIT_FAILURE);
    }

    unlink(path);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        syslog(LOG_ERR, "Failed bind socket");
        exit(EXIT_FAILURE);
    }

    listen(fd, 1);
    return fd;
}

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

static void sysfs_read_temp(const char* path, int * temp)
{
    char tmp_buf[25];
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        syslog(LOG_ERR, "Impossible d'ouvrir %s : %m", path);
        return;
    }

    ssize_t nr = read(fd, tmp_buf, sizeof(tmp_buf) - 1);
    close(fd);

    if (nr < 0) {
        syslog(LOG_ERR, "Erreur de lecture sur %s : %m", path);
        return;
    }

    tmp_buf[nr] = '\0';

    int ret = sscanf(tmp_buf,"%d",temp);
    if (ret != 1)
    {
        syslog(LOG_ERR, "Impossible de parser la température depuis %s", path);
        return ;
    }

    syslog(LOG_INFO, "Température lue avec succès : %d", *temp);
    

}

static void sysfs_write_conf(const char* path, module_config_t conf) {
    char tmp_buf[25];
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        syslog(LOG_ERR, "Impossible d'ouvrir %s : %m", path);
        return;
    }
    int len = snprintf(tmp_buf, sizeof(tmp_buf), "%s %d %d\n",
                       str_mode_option[conf.mode], conf.frequency, conf.duty);

    if (len < 0 || len >= sizeof(tmp_buf)) {
        syslog(LOG_ERR,
               "Erreur : Données de configuration trop longues pour le buffer");
        close(fd);  // Toujours fermer avant de partir
        return;
    }

    if (write(fd, tmp_buf, len) < 0) {
        syslog(LOG_ERR, "Erreur d'écriture dans %s : %m", path);
    } else {
        syslog(LOG_INFO, "Write config: %s in \n%s", tmp_buf, path);
    }

    close(fd);
}

void catch_signal(int signo) { printf("Signal [%d] arrived: \n", signo); }

void demon(void) {
    int ret;
    struct itimerspec periode = {.it_interval = {5, 0}, .it_value = {5, 0}};

    ssd1306_init();
    ssd1306_set_position(0, 0);
    ssd1306_puts("CSEL RIVIER :07");
    ssd1306_set_position(0, 1);
    ssd1306_puts("Demon      v0.1");
    ssd1306_set_position(0, 2);
    ssd1306_puts("--------------");

    int fd_led = open_led();
    syslog(LOG_INFO, "LED open: fd %d\n", fd_led);
    int fd_btnA0 = open_bnt(BTN_A0);
    syslog(LOG_INFO, "BTN open: fd %d\n", fd_btnA0);
    int fd_btnA2 = open_bnt(BTN_A2);
    syslog(LOG_INFO, "BTN open: fd %d\n", fd_btnA2);
    int fd_btnA3 = open_bnt(BTN_A3);
    syslog(LOG_INFO, "BTN open: fd %d\n", fd_btnA3);

    int fd_server = create_unix_socket(SOCKET_PATH);

    int fd_timer = timerfd_create(CLOCK_MONOTONIC, 0);
    if (fd_timer == -1) {
        syslog(LOG_ERR, "Failed create timerfd_settime");
        exit(EXIT_FAILURE);
    }

    if (timerfd_settime(fd_timer, 0, &periode, NULL) == -1) {
        syslog(LOG_ERR, "Failed settime timerfd_settime");
        exit(EXIT_FAILURE);
    }

    int fd_ep = epoll_create1(0);
    if (fd_ep == -1) {
        syslog(LOG_ERR, "Failed creat epoll");
        exit(EXIT_FAILURE);
    }

    struct epoll_event events[MAX_EVENTS];

    register_event(fd_ep, &ctx_btns[0], fd_btnA0);
    register_event(fd_ep, &ctx_btns[1], fd_btnA2);
    register_event(fd_ep, &ctx_btns[2], fd_btnA3);
    register_event(fd_ep, &ctx_socket_srv, fd_server);
    register_event(fd_ep, &ctx_timer, fd_timer);

    while (1) {
        int n = epoll_wait(fd_ep, events, MAX_EVENTS, -1);
        if (n == -1) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "Failed receive events");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < n; i++) {
            EventContext* ctx = (EventContext*)events[i].data.ptr;
            ctx->handler(ctx->fd, ctx);
        }
    }
}

int main(int argc, char* argv[]) {
    int ret;

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
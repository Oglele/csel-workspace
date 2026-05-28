#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#include "commun.h"

#define SOCKET_PATH "/tmp/demon.sock"




int match_string(const char *const *array, const char *string) {
    for (int i = 0; array[i] != NULL; i++) {
        if (strcmp(array[i], string) == 0) {
            return i;
        }
    }
    return -EINVAL;
}

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_un addr;
    char message[25], tmp_buf[10];
    module_config_t conf_tmp;

    if (argc < 4)
    {
        printf("User app - Usage\n"
               "user <mode> <frequency> <dutyCycle>\n"
               "    mode:       manual / auto\n"
               "    frequency:  min 1, max 20\n"
               "    duty:       0-100\n\n");
        exit(0);
    }

    strncpy(tmp_buf, argv[1], sizeof(tmp_buf) - 1);
    tmp_buf[sizeof(tmp_buf) - 1] = '\0';
    conf_tmp.frequency = atoi(argv[2]);
    conf_tmp.duty = atoi(argv[3]);


    int idx = match_string(str_mode_option, tmp_buf);
    if (idx < 0) {
        fprintf(stderr, "Mode invalide : 'auto' ou 'manual' uniquement\n");
        return -EINVAL;
    }

    conf_tmp.frequency = clamp(conf_tmp.frequency, FREQ_MIN, FREQ_MAX);
    conf_tmp.duty = clamp(conf_tmp.duty, 0, 100);

    snprintf(message, sizeof(message), "%s %d %d", tmp_buf, conf_tmp.frequency, conf_tmp.duty);

    printf("msg send = %s %d %d\n",tmp_buf, conf_tmp.frequency, conf_tmp.duty);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        err(EXIT_FAILURE, "connot creat socket");
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        err(EXIT_FAILURE, "Connot connect to demon");
    }

    if (write(sockfd, message, strlen(message)) < 0) {
        err(EXIT_FAILURE, "Connot write message");
    } else {
        printf("Message envoyé au démon : %s\n", message);
    }

    close(sockfd);
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

#define SOCKET_PATH "/tmp/demon.sock"

int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_un addr;
    char *message = (argc > 1) ? argv[1] : "Hello Demon!";

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
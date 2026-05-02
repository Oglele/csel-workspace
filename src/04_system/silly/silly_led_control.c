/**
 * Copyright 2018 University of Applied Sciences Western Switzerland / Fribourg
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Project: HEIA-FR / HES-SO MSE - MA-CSEL1 Laboratory
 *
 * Abstract: System programming -  file system
 *
 * Purpose: NanoPi silly status led control system
 *
 * Autĥor:  Daniel Gachet
 * Date:    07.11.2018
 */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/*
 * status led - gpioa.10 --> gpio10
 * power led  - gpiol.10 --> gpio362
 */
#define GPIO_EXPORT "/sys/class/gpio/export"
#define GPIO_UNEXPORT "/sys/class/gpio/unexport"
#define GPIO_LED "/sys/class/gpio/gpio10"

#define GPIO_BTN_A0 "/sys/class/gpio/gpio0"
#define GPIO_BTN_A2 "/sys/class/gpio/gpio2"
#define GPIO_BTN_A3 "/sys/class/gpio/gpio3"

#define LED "10"
#define BTN_A0 "0"
#define BTN_A2 "2"
#define BTN_A3 "3"

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

int main(int argc, char* argv[]) {
    // Timer part
    struct itimerspec new_value;
    long init_period;
    uint64_t exp;
    int timer_fd;
    ssize_t s;
    bool led_v = 0;

    long duty = 50;      // %
    long period = 1000;  // ms
    period *= 1000000;   // in ns
    init_period = period;

    // compute duty period...
    long p1 = period / 100 * duty;
    long p2 = period - p1;

    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;

    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_nsec = p1;

    timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timer_fd == -1) err(EXIT_FAILURE, "timerfd_create");

    if (timerfd_settime(timer_fd, 0, &new_value, NULL) == -1)
        err(EXIT_FAILURE, "timerfd_settime");

    int led = open_led();
    char buf[100];

    int btnA0 = open_bnt(BTN_A0);
    int btnA2 = open_bnt(BTN_A2);
    int btnA3 = open_bnt(BTN_A3);


    // Sellector part
    fd_set fd_in, fd_out,fd_exc;

    // Trouver le plus grand FD parmi les 4
    int max_fd = btnA0;
    if (btnA2 > max_fd) max_fd = btnA2;
    if (btnA3 > max_fd) max_fd = btnA3;
    if (timer_fd > max_fd) max_fd = timer_fd;

    

    while (1) {
        FD_ZERO(&fd_in);

        // monitor fd1 for input events and fd2 for output events
        FD_SET(timer_fd, &fd_in);
        FD_SET(btnA0, &fd_exc);
        FD_SET(btnA2, &fd_exc);
        FD_SET(btnA3, &fd_exc);
        // FD_SET(fd2, &fd_out);

        // wait up to 5 seconds
        struct timeval tv = {
            .tv_sec = 5,
            .tv_usec = 0,
        };
        int ret = select(max_fd + 1, &fd_in, NULL, &fd_exc, &tv);

        if (ret > 0) {
            // Logique timer
            if (FD_ISSET(timer_fd, &fd_in)) {
                s = read(timer_fd, &exp, sizeof(uint64_t));
                if (s != sizeof(uint64_t)) {
                    err(EXIT_FAILURE, "read");
                }

                led_v = !led_v;
                if (led_v == 1) {
                    pwrite(led, "1", sizeof("1"), 0);
                } else {
                    pwrite(led, "0", sizeof("0"), 0);
                }
                long next_duration = led_v ? p2 : p1;
                new_value.it_value.tv_nsec = next_duration;

                if (timerfd_settime(timer_fd, 0, &new_value, NULL) == -1)
                    err(EXIT_FAILURE, "timerfd_settime (loop)");
            }
            if (FD_ISSET(btnA0, &fd_exc)) {
                lseek(btnA0, 0, SEEK_SET);
                if (read(btnA0, buf, sizeof(buf)) > 0) {
                    printf("bt1: %s\n",buf);
                    period += 10000000;
                }
                
            }

            if (FD_ISSET(btnA2, &fd_exc)) {
                lseek(btnA2, 0, SEEK_SET);
                    if (read(btnA2, buf, sizeof(buf)) > 0) {
                        printf("bt2: %s\n",buf);
                        period = init_period;
                }
            }

            if (FD_ISSET(btnA3, &fd_exc)) {
                lseek(btnA3, 0, SEEK_SET);
                    if (read(btnA3, buf, sizeof(buf)) > 0) {
                        printf("bt3: %s p %d\n",buf,period);
                        period -= 10000000;
                }
            }
            p1 = period / 100 * duty;
            p2 = period - p1;
        }
    }

    return 0;
}
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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <stdint.h>
#include <stdbool.h>

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

static int open_led()
{
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

static int open_bnt(char* num_port)
{
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

int main(int argc, char* argv[])
{
    struct itimerspec  new_value;
    uint64_t exp;
    int timer_fd;
    ssize_t s;
    bool led_v = 0;

    long duty   = 50;     // %
    long period = 1000;  // ms
    period *= 1000000;  // in ns

    // compute duty period...
    long p1 = period / 100 * duty;
    long p2 = period - p1;

    int led = open_led();

    new_value.it_interval.tv_sec = 0;
    new_value.it_interval.tv_nsec = 0;
    
    new_value.it_value.tv_sec = 0;
    new_value.it_value.tv_nsec = p1;

    timer_fd = timerfd_create(CLOCK_MONOTONIC,0);
    if (timer_fd == -1)
        err(EXIT_FAILURE, "timerfd_create");

    if (timerfd_settime(timer_fd, 0, &new_value, NULL) == -1)
               err(EXIT_FAILURE, "timerfd_settime");

    int btnA0 = open_bnt(BTN_A0);
    int btnA2 = open_bnt(BTN_A2);
    int btnA3 = open_bnt(BTN_A3);


    int k = 0;
    while (1) {

        // long delta =
        //     (t2.tv_sec - t1.tv_sec) * 1000000000 + (t2.tv_nsec - t1.tv_nsec);

        s = read(timer_fd, &exp, sizeof(uint64_t));
        if (s != sizeof(uint64_t))
                   err(EXIT_FAILURE, "read");

        led_v = !led_v;
            if (led_v == 1)
                pwrite(led, "1", sizeof("1"), 0);
            else
                pwrite(led, "0", sizeof("0"), 0);

        long next_duration = led_v ? p2 : p1;
        new_value.it_value.tv_nsec = next_duration;

        timerfd_settime(timer_fd, 0, &new_value, NULL);
        if (timerfd_settime(timer_fd, 0, &new_value, NULL) == -1)
            err(EXIT_FAILURE, "timerfd_settime (loop)");
        }

    return 0;
}
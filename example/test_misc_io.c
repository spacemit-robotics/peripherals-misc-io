/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <misc_io.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile int g_count = 0;

static void cb(struct misc_dev *dev, enum misc_event ev, uint64_t timestamp_us, void *args)
{
    (void)dev;
    (void)args;
    g_count++;
    if (ev != MISC_EV_ACTIVE)
        return;

    printf("[cb] count=%d ev=%s timestamp_us=%" PRIu64 "\n",
        g_count, (ev == MISC_EV_ACTIVE) ? "ACTIVE" : "INACTIVE",
        timestamp_us);
    fflush(stdout);
}

int test_trigger(void)
{
    struct misc_gpiod_ctx ctx = {
        .chip_name = "gpiochip2",
        .line_offset = 19,
    };

    struct misc_dev *dev = misc_io_alloc(MISC_TYPE_GENERIC, MISC_DIR_INPUT, &ctx);
    if (!dev) {
        fprintf(stderr, "misc_io_alloc failed\n");
        return 1;
    }

    misc_io_config(dev, MISC_ACTIVE_HIGH, 0/*debounce ms*/);
    misc_io_trigger(dev, cb, NULL);

    /* do something */
    while (1) {
        sleep(1);
    }

    misc_io_free(dev);
}


int test_get_io(void)
{
    struct misc_gpiod_ctx ctx = {
        .chip_name = "gpiochip0",
        .line_offset = 61,
    };

    struct misc_dev *dev = misc_io_alloc(MISC_TYPE_GENERIC, MISC_DIR_INPUT, &ctx);
    if (!dev) {
        fprintf(stderr, "misc_io_alloc failed\n");
        return 1;
    }

    int get_value = 0;
    for (int i = 0; i < 50; i++) {
        get_value = misc_io_get(dev);
        printf("get_value:%d\n", get_value);
        sleep(1);
    }

    misc_io_free(dev);
}

int test_set_io(void)
{
    struct misc_gpiod_ctx ctx = {
        .chip_name = "gpiochip0",
        .line_offset = 9,
    };

    struct misc_dev *dev = misc_io_alloc(MISC_TYPE_GENERIC, MISC_DIR_OUTPUT, &ctx);
    if (!dev) {
        fprintf(stderr, "misc_io_alloc failed\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        printf("set hight \n");
        misc_io_set(dev, MISC_ACTIVE_HIGH);
        sleep(2);
        printf("set low \n");
        misc_io_set(dev, MISC_ACTIVE_LOW);
        sleep(2);
    }

    misc_io_free(dev);
}

int main(int argc, char **argv)
{
    test_trigger();

    // test_set_io();

    // test_get_io();

    return 0;
}

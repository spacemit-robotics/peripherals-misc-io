/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <misc_io.h>

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile int g_count = 0;

static int parse_line_offset(const char *text, unsigned int *line)
{
    const char *digits = text;
    char *end = NULL;
    unsigned long parsed;
    int base = 10;

    if (!text || !*text || !line)
        return -1;

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        digits += 2;
        base = 16;
    }
    if (!*digits)
        return -1;

    for (const char *p = digits; *p; ++p) {
        if (base == 10 ? !isdigit((unsigned char)*p) :
            !isxdigit((unsigned char)*p))
            return -1;
    }

    errno = 0;
    parsed = strtoul(digits, &end, base);
    if (errno == ERANGE || end == digits || *end != '\0' ||
        parsed > UINT_MAX)
        return -1;

    *line = (unsigned int)parsed;
    return 0;
}

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

int test_trigger(const char *chip, unsigned int line)
{
    struct misc_gpiod_ctx ctx = {
        .chip_name = chip,
        .line_offset = line,
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
    return 0;
}


int test_get_io(const char *chip, unsigned int line)
{
    struct misc_gpiod_ctx ctx = {
        .chip_name = chip,
        .line_offset = line,
    };

    struct misc_dev *dev = misc_io_alloc(MISC_TYPE_GENERIC, MISC_DIR_INPUT, &ctx);
    if (!dev) {
        fprintf(stderr, "misc_io_alloc failed\n");
        return 1;
    }

    int get_value = 0;
    for (int i = 0; i < 50; i++) {
        get_value = misc_io_get(dev);
        if (get_value < 0) {
            fprintf(stderr, "misc_io_get failed: %d\n", get_value);
            misc_io_free(dev);
            return 1;
        }
        printf("get_value:%d\n", get_value);
        sleep(1);
    }

    misc_io_free(dev);
    return 0;
}

int test_set_io(const char *chip, unsigned int line)
{
    struct misc_gpiod_ctx ctx = {
        .chip_name = chip,
        .line_offset = line,
    };

    struct misc_dev *dev = misc_io_alloc(MISC_TYPE_GENERIC, MISC_DIR_OUTPUT, &ctx);
    if (!dev) {
        fprintf(stderr, "misc_io_alloc failed\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        int ret;

        printf("set high\n");
        ret = misc_io_set(dev, MISC_ACTIVE_HIGH);
        if (ret != 0) {
            fprintf(stderr, "misc_io_set high failed: %d\n", ret);
            misc_io_free(dev);
            return 1;
        }
        sleep(2);
        printf("set low\n");
        ret = misc_io_set(dev, MISC_ACTIVE_LOW);
        if (ret != 0) {
            fprintf(stderr, "misc_io_set low failed: %d\n", ret);
            misc_io_free(dev);
            return 1;
        }
        sleep(2);
    }

    misc_io_free(dev);
    return 0;
}

/*
 * 用法: test_misc_io [trigger|get|set] [chip_name] [line_offset]
 * 默认: trigger gpiochip2 19；get gpiochip0 61；set gpiochip0 9
 * MUSE Pi Pro 只有 gpiochip0，line_offset 即 GPIO 编号，例如:
 * test_misc_io trigger gpiochip0 113
 */
int main(int argc, char **argv)
{
    const char *mode = (argc >= 2) ? argv[1] : "trigger";
    const char *chip = NULL;
    unsigned int line = 0;

    if (argc > 4) {
        fprintf(stderr, "usage: %s [trigger|get|set] [chip_name] [line_offset]\n",
            argv[0]);
        return 2;
    }

    if (strcmp(mode, "trigger") == 0) {
        chip = "gpiochip2";
        line = 19;
    } else if (strcmp(mode, "get") == 0) {
        chip = "gpiochip0";
        line = 61;
    } else if (strcmp(mode, "set") == 0) {
        chip = "gpiochip0";
        line = 9;
    } else {
        fprintf(stderr, "usage: %s [trigger|get|set] [chip_name] [line_offset]\n", argv[0]);
        return 1;
    }

    if (argc >= 3)
        chip = argv[2];
    if (argc >= 4 && parse_line_offset(argv[3], &line) != 0) {
        fprintf(stderr,
            "line_offset 必须是有效的非负整数（支持十进制或十六进制）\n");
        return 2;
    }

    if (strcmp(mode, "trigger") == 0)
        return test_trigger(chip, line);
    if (strcmp(mode, "get") == 0)
        return test_get_io(chip, line);
    return test_set_io(chip, line);
}

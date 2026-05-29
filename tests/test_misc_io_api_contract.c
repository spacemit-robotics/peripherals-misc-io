/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "misc_io.h"
#include "gpiod.h"

static int g_failures;

#define CHECK_TRUE(expr) do { \
    if (!(expr)) { \
        printf("FAIL:%s:%d: expected true: %s\n", __FILE__, __LINE__, #expr); \
        g_failures++; \
    } \
} while (0)

#define CHECK_INT_EQ(actual, expected) do { \
    int _actual = (int)(actual); \
    int _expected = (int)(expected); \
    if (_actual != _expected) { \
        printf("FAIL:%s:%d: expected %s == %d, got %d\n", \
            __FILE__, __LINE__, #actual, _expected, _actual); \
        g_failures++; \
    } \
} while (0)

static void reset_test_state(void)
{
    g_failures = 0;
    fake_gpiod_reset();
}

static void unused_cb(struct misc_dev *dev, enum misc_event ev,
    uint64_t timestamp_us, void *args)
{
    (void)dev;
    (void)ev;
    (void)timestamp_us;
    (void)args;
}

static void test_error_paths(void)
{
    struct misc_gpiod_ctx bad_ctx = {
        .chip_name = "gpiochip9",
        .line_offset = 1,
        .consumer = "misc-test",
    };

    CHECK_TRUE(misc_io_alloc(MISC_TYPE_GENERIC, MISC_DIR_OUTPUT, NULL) == NULL);
    CHECK_TRUE(misc_io_alloc(MISC_TYPE_GENERIC, MISC_DIR_OUTPUT, &bad_ctx) == NULL);
    CHECK_INT_EQ(misc_io_set(NULL, true), -EINVAL);
    CHECK_INT_EQ(misc_io_get(NULL), -EINVAL);
    misc_io_config(NULL, MISC_ACTIVE_HIGH, 10);
    misc_io_trigger(NULL, unused_cb, NULL);
    misc_io_free(NULL);
}

static void test_functional(void)
{
    struct misc_gpiod_ctx out_ctx = {
        .chip_name = "gpiochip0",
        .line_offset = 3,
        .consumer = "misc-test-out",
    };
    struct misc_gpiod_ctx in_ctx = {
        .chip_name = "gpiochip0",
        .line_offset = 4,
        .consumer = "misc-test-in",
    };
    struct misc_dev *out_dev;
    struct misc_dev *in_dev;

    out_dev = misc_io_alloc(MISC_TYPE_RELAY, MISC_DIR_OUTPUT, &out_ctx);
    CHECK_TRUE(out_dev != NULL);
    if (!out_dev)
        return;

    CHECK_INT_EQ(fake_gpiod_get_value(out_ctx.line_offset), 0);
    CHECK_INT_EQ(misc_io_set(out_dev, true), 0);
    CHECK_INT_EQ(fake_gpiod_get_value(out_ctx.line_offset), 1);
    CHECK_INT_EQ(misc_io_get(out_dev), 1);
    misc_io_config(out_dev, MISC_ACTIVE_LOW, 5);
    CHECK_INT_EQ(misc_io_set(out_dev, true), 0);
    CHECK_INT_EQ(fake_gpiod_get_value(out_ctx.line_offset), 0);
    CHECK_INT_EQ(misc_io_get(out_dev), 1);
    CHECK_INT_EQ(misc_io_set(out_dev, false), 0);
    CHECK_INT_EQ(fake_gpiod_get_value(out_ctx.line_offset), 1);
    CHECK_INT_EQ(misc_io_get(out_dev), 0);
    misc_io_trigger(out_dev, unused_cb, NULL);
    misc_io_free(out_dev);

    fake_gpiod_set_value(in_ctx.line_offset, 1);
    in_dev = misc_io_alloc(MISC_TYPE_SWITCH, MISC_DIR_INPUT, &in_ctx);
    CHECK_TRUE(in_dev != NULL);
    if (!in_dev)
        return;
    misc_io_config(in_dev, MISC_ACTIVE_HIGH, 0);
    CHECK_INT_EQ(misc_io_get(in_dev), 1);
    fake_gpiod_set_value(in_ctx.line_offset, 0);
    CHECK_INT_EQ(misc_io_get(in_dev), 0);
    CHECK_INT_EQ(misc_io_set(in_dev, true), -EPERM);
    misc_io_free(in_dev);

    CHECK_INT_EQ(fake_gpiod_get_release_count(), 2);
}

static int finish_test(const char *name)
{
    if (g_failures != 0) {
        printf("%s FAILED: %d failure(s)\n", name, g_failures);
        return 1;
    }
    printf("%s PASSED\n", name);
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = (argc > 1) ? argv[1] : "all";

    if (strcmp(mode, "functional") == 0) {
        reset_test_state();
        test_functional();
        return finish_test("misc_io api functional test");
    }
    if (strcmp(mode, "error-paths") == 0) {
        reset_test_state();
        test_error_paths();
        return finish_test("misc_io api error paths test");
    }
    if (strcmp(mode, "all") == 0) {
        reset_test_state();
        test_functional();
        if (finish_test("misc_io api functional test") != 0)
            return 1;
        reset_test_state();
        test_error_paths();
        if (finish_test("misc_io api error paths test") != 0)
            return 1;
        printf("misc_io api contract test PASSED\n");
        return 0;
    }

    fprintf(stderr, "usage: %s [all|functional|error-paths]\n", argv[0]);
    return 2;
}

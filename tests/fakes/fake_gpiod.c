/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gpiod.h"

#include <stddef.h>
#include <string.h>

#define MAX_FAKE_LINES 256

struct gpiod_chip {
    int id;
};

struct gpiod_line {
    unsigned int offset;
    int requested;
};

static struct gpiod_chip g_chip0 = { .id = 0 };
static struct gpiod_line g_lines[MAX_FAKE_LINES];
static int g_values[MAX_FAKE_LINES];
static int g_release_count;

void fake_gpiod_reset(void)
{
    memset(g_lines, 0, sizeof(g_lines));
    memset(g_values, 0, sizeof(g_values));
    g_release_count = 0;
}

void fake_gpiod_set_value(unsigned int offset, int value)
{
    if (offset < MAX_FAKE_LINES)
        g_values[offset] = value ? 1 : 0;
}

int fake_gpiod_get_value(unsigned int offset)
{
    if (offset >= MAX_FAKE_LINES)
        return -1;
    return g_values[offset];
}

int fake_gpiod_get_release_count(void)
{
    return g_release_count;
}

struct gpiod_chip *gpiod_chip_open_by_name(const char *name)
{
    if (!name)
        return NULL;
    if (strcmp(name, "gpiochip0") == 0)
        return &g_chip0;
    return NULL;
}

struct gpiod_chip *gpiod_chip_open(const char *path)
{
    if (!path)
        return NULL;
    if (strcmp(path, "/dev/gpiochip0") == 0)
        return &g_chip0;
    return NULL;
}

void gpiod_chip_close(struct gpiod_chip *chip)
{
    (void)chip;
}

struct gpiod_line *gpiod_chip_get_line(struct gpiod_chip *chip, unsigned int offset)
{
    (void)chip;

    if (offset >= MAX_FAKE_LINES)
        return NULL;

    g_lines[offset].offset = offset;
    return &g_lines[offset];
}

int gpiod_line_request_both_edges_events(struct gpiod_line *line, const char *consumer)
{
    (void)consumer;
    if (!line)
        return -1;
    line->requested = 1;
    return 0;
}

int gpiod_line_request_input(struct gpiod_line *line, const char *consumer)
{
    (void)consumer;
    if (!line)
        return -1;
    line->requested = 1;
    return 0;
}

int gpiod_line_request_output(struct gpiod_line *line, const char *consumer,
    int default_value)
{
    (void)consumer;
    if (!line || line->offset >= MAX_FAKE_LINES)
        return -1;
    line->requested = 1;
    g_values[line->offset] = default_value ? 1 : 0;
    return 0;
}

int gpiod_line_get_value(struct gpiod_line *line)
{
    if (!line || !line->requested || line->offset >= MAX_FAKE_LINES)
        return -1;
    return g_values[line->offset];
}

int gpiod_line_set_value(struct gpiod_line *line, int value)
{
    if (!line || !line->requested || line->offset >= MAX_FAKE_LINES)
        return -1;
    g_values[line->offset] = value ? 1 : 0;
    return 0;
}

int gpiod_line_event_wait(struct gpiod_line *line, const struct timespec *timeout)
{
    (void)line;
    (void)timeout;
    return 0;
}

int gpiod_line_event_read(struct gpiod_line *line, struct gpiod_line_event *event)
{
    (void)line;
    (void)event;
    return -1;
}

void gpiod_line_release(struct gpiod_line *line)
{
    if (!line)
        return;
    line->requested = 0;
    g_release_count++;
}

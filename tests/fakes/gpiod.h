/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef GPIOD_H
#define GPIOD_H

#include <time.h>

struct gpiod_chip;
struct gpiod_line;

struct gpiod_line_event {
    int event_type;
    struct timespec ts;
};

#define GPIOD_LINE_EVENT_RISING_EDGE 1
#define GPIOD_LINE_EVENT_FALLING_EDGE 2

void fake_gpiod_reset(void);
void fake_gpiod_set_value(unsigned int offset, int value);
int fake_gpiod_get_value(unsigned int offset);
int fake_gpiod_get_release_count(void);

struct gpiod_chip *gpiod_chip_open_by_name(const char *name);
struct gpiod_chip *gpiod_chip_open(const char *path);
void gpiod_chip_close(struct gpiod_chip *chip);
struct gpiod_line *gpiod_chip_get_line(struct gpiod_chip *chip, unsigned int offset);
int gpiod_line_request_both_edges_events(struct gpiod_line *line, const char *consumer);
int gpiod_line_request_input(struct gpiod_line *line, const char *consumer);
int gpiod_line_request_output(struct gpiod_line *line, const char *consumer,
    int default_value);
int gpiod_line_get_value(struct gpiod_line *line);
int gpiod_line_set_value(struct gpiod_line *line, int value);
int gpiod_line_event_wait(struct gpiod_line *line, const struct timespec *timeout);
int gpiod_line_event_read(struct gpiod_line *line, struct gpiod_line_event *event);
void gpiod_line_release(struct gpiod_line *line);

#endif /* GPIOD_H */

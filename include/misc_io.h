/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef MISC_IO_H
#define MISC_IO_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* device type (for debug/logging) */
enum misc_type {
    MISC_TYPE_GENERIC = 0,
    MISC_TYPE_BUZZER,
    MISC_TYPE_RELAY,
    MISC_TYPE_SWITCH,
    MISC_TYPE_SENSOR,
};

/* direction */
enum misc_dir {
    MISC_DIR_INPUT = 0,
    MISC_DIR_OUTPUT,
};

/* active logic */
enum misc_logic {
    MISC_ACTIVE_LOW = 0,
    MISC_ACTIVE_HIGH
};

/* event */
enum misc_event {
    MISC_EV_ACTIVE = 0,
    MISC_EV_INACTIVE,
};

/* hw_ctx for misc_io_alloc(): */
struct misc_gpiod_ctx {
    const char *chip_name;       /* e.g. "gpiochip0" */
    unsigned int line_offset;    /* line offset within chip */
    const char *consumer;        /* optional, can be NULL */
};

struct misc_dev; /* opaque handle */

/* callback function */
typedef void (*misc_cb_t)(struct misc_dev *dev, enum misc_event ev, void *args);

/* allocate device */
struct misc_dev *misc_io_alloc(enum misc_type type, enum misc_dir dir, void *hw_ctx);

/* configure active logic and debounce */
void misc_io_config(struct misc_dev *dev, enum misc_logic active_logic, uint16_t debounce_ms);

/* output API */
int misc_io_set(struct misc_dev *dev, bool active);

/* input API */
int misc_io_get(struct misc_dev *dev);

/* start input monitoring and invoke cb on debounced state changes */
void misc_io_trigger(struct misc_dev *dev, misc_cb_t cb, void *args);

/* free device */
void misc_io_free(struct misc_dev *dev);

#ifdef __cplusplus
}
#endif

#endif /* MISC_IO_H */

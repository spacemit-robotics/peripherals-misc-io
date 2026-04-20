/*
 * Copyright (C) 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */
#include <misc_io.h>

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include <gpiod.h>


/* ---- internal helpers ---- */

static uint64_t now_ms_monotonic(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

static enum misc_event value_to_event(struct misc_dev *dev, int raw_value);

/* ---- misc_dev ---- */

struct misc_dev {
    enum misc_type type;
    enum misc_dir dir;

    enum misc_logic active_logic;
    uint16_t debounce_ms;

    /* gpiod objects */
    struct gpiod_chip *chip;
#if defined(LIBGPIOD_V2)
    unsigned int offset;
    struct gpiod_line_settings *settings;
    struct gpiod_line_config *lcfg;
    struct gpiod_request_config *rcfg;
    struct gpiod_line_request *req;
#else
    struct gpiod_line *line;
#endif

    /* trigger thread */
    pthread_t th;
    pthread_mutex_t lock; /* protect cb + running + last_state + timing */
    int running;
    int thread_started;

    misc_cb_t cb;
    void *cb_args;

    /* last known raw value (0/1), and debounce bookkeeping */
    int last_raw;
    uint64_t last_change_ms;
};

#if defined(LIBGPIOD_V2)
static int request_line_common(struct misc_dev *dev,
    const char *consumer,
    enum gpiod_line_direction direction,
    enum gpiod_line_value default_value)
{
    dev->settings = gpiod_line_settings_new();
    if (!dev->settings)
        return -ENOMEM;

    gpiod_line_settings_set_direction(dev->settings, direction);
    if (direction == GPIOD_LINE_DIRECTION_OUTPUT)
        gpiod_line_settings_set_output_value(dev->settings, default_value);

    dev->lcfg = gpiod_line_config_new();
    if (!dev->lcfg) {
        gpiod_line_settings_free(dev->settings);
        dev->settings = NULL;
        return -ENOMEM;
    }

    unsigned int offset = dev->offset;
    if (gpiod_line_config_add_line_settings(dev->lcfg, &offset, 1, dev->settings) < 0) {
        gpiod_line_config_free(dev->lcfg);
        gpiod_line_settings_free(dev->settings);
        dev->lcfg = NULL;
        dev->settings = NULL;
        return -errno;
    }

    dev->rcfg = gpiod_request_config_new();
    if (!dev->rcfg) {
        gpiod_line_config_free(dev->lcfg);
        gpiod_line_settings_free(dev->settings);
        dev->lcfg = NULL;
        dev->settings = NULL;
        return -ENOMEM;
    }
    gpiod_request_config_set_consumer(dev->rcfg, consumer ? consumer : "misc_io");

    dev->req = gpiod_chip_request_lines(dev->chip, dev->rcfg, dev->lcfg);
    if (!dev->req) {
        gpiod_request_config_free(dev->rcfg);
        gpiod_line_config_free(dev->lcfg);
        gpiod_line_settings_free(dev->settings);
        dev->rcfg = NULL;
        dev->lcfg = NULL;
        dev->settings = NULL;
        return -errno;
    }

    return 0;
}

static int request_line_input_events(struct misc_dev *dev, const char *consumer)
{
    return request_line_common(dev, consumer,
        GPIOD_LINE_DIRECTION_INPUT,
        GPIOD_LINE_VALUE_INACTIVE);
}

static int request_line_input(struct misc_dev *dev, const char *consumer)
{
    return request_line_common(dev, consumer,
        GPIOD_LINE_DIRECTION_INPUT,
        GPIOD_LINE_VALUE_INACTIVE);
}

static int request_line_output(struct misc_dev *dev, const char *consumer, int default_value)
{
    enum gpiod_line_value v = default_value ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    return request_line_common(dev, consumer, GPIOD_LINE_DIRECTION_OUTPUT, v);
}
#else
static int request_line_input_events(struct misc_dev *dev, const char *consumer)
{
    /* libgpiod v1: request edge events for input line */
    int ret = gpiod_line_request_both_edges_events(dev->line, consumer ? consumer : "misc_io");
    if (ret < 0)
        return -errno;
    return 0;
}

static int request_line_input(struct misc_dev *dev, const char *consumer)
{
    int ret = gpiod_line_request_input(dev->line, consumer ? consumer : "misc_io");
    if (ret < 0)
        return -errno;
    return 0;
}

static int request_line_output(struct misc_dev *dev, const char *consumer, int default_value)
{
    int ret = gpiod_line_request_output(dev->line, consumer ? consumer : "misc_io", default_value);
    if (ret < 0)
        return -errno;
    return 0;
}
#endif

static int read_raw_value(struct misc_dev *dev)
{
#if defined(LIBGPIOD_V2)
    enum gpiod_line_value v = gpiod_line_request_get_value(dev->req, dev->offset);
    if ((int)v < 0)
        return -errno;
    return (v == GPIOD_LINE_VALUE_ACTIVE) ? 1 : 0;
#else
    int v = gpiod_line_get_value(dev->line);
    if (v < 0)
        return -errno;
    return v;
#endif
}

static int write_raw_value(struct misc_dev *dev, int raw)
{
#if defined(LIBGPIOD_V2)
    enum gpiod_line_value v = raw ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
    if (gpiod_line_request_set_value(dev->req, dev->offset, v) < 0)
        return -errno;
    return 0;
#else
    if (gpiod_line_set_value(dev->line, raw) < 0)
        return -errno;
    return 0;
#endif
}

static enum misc_event value_to_event(struct misc_dev *dev, int raw_value)
{
    /* active_logic defines which raw level maps to ACTIVE */
    int active_raw = (dev->active_logic == MISC_ACTIVE_HIGH) ? 1 : 0;
    return (raw_value == active_raw) ? MISC_EV_ACTIVE : MISC_EV_INACTIVE;
}

static void *trigger_thread_fn(void *arg)
{
    struct misc_dev *dev = (struct misc_dev *)arg;

    /* Initialize last_raw from current value if possible */
    int v = read_raw_value(dev);
    pthread_mutex_lock(&dev->lock);
    dev->last_raw = v;
    dev->last_change_ms = now_ms_monotonic();
    pthread_mutex_unlock(&dev->lock);

#if defined(LIBGPIOD_V2)
    const uint64_t poll_interval_ms = 10;
#endif

    while (1) {
        pthread_mutex_lock(&dev->lock);
        int running = dev->running;
        uint16_t debounce_ms = dev->debounce_ms;
        misc_cb_t cb = dev->cb;
        void *cb_args = dev->cb_args;
        pthread_mutex_unlock(&dev->lock);

        if (!running)
            break;

#if defined(LIBGPIOD_V2)
        int raw_now = read_raw_value(dev);
        if (raw_now < 0) {
            usleep(poll_interval_ms * 1000);
            continue;
        }

        uint64_t tnow = now_ms_monotonic();

        /* Debounce: ignore transitions within debounce window */
        int do_cb = 0;
        enum misc_event me = MISC_EV_INACTIVE;

        pthread_mutex_lock(&dev->lock);
        if (dev->last_raw != raw_now) {
            uint64_t dt = tnow - dev->last_change_ms;
            if (debounce_ms == 0 || dt >= (uint64_t)debounce_ms) {
                dev->last_raw = raw_now;
                dev->last_change_ms = tnow;
                do_cb = (cb != NULL);
                me = value_to_event(dev, raw_now);
            }
        }
        pthread_mutex_unlock(&dev->lock);

        if (do_cb) {
            cb(dev, me, cb_args);
        }

        usleep(poll_interval_ms * 1000);
#else
        /* Wait for an edge event with timeout, so we can exit promptly */
        struct timespec timeout;
        timeout.tv_sec = 1;
        timeout.tv_nsec = 0;

        int ret = gpiod_line_event_wait(dev->line, &timeout);
        if (ret < 0) {
            /* error, keep looping but allow exit */
            continue;
        }
        if (ret == 0) {
            /* timeout */
            continue;
        }

        struct gpiod_line_event ev;
        if (gpiod_line_event_read(dev->line, &ev) < 0) {
            continue;
        }

        /* Read current stable value immediately after edge */
        int raw_now = read_raw_value(dev);
        uint64_t tnow = now_ms_monotonic();

        /* Debounce: ignore transitions within debounce window */
        int do_cb = 0;
        enum misc_event me = MISC_EV_INACTIVE;

        pthread_mutex_lock(&dev->lock);
        if (dev->last_raw != raw_now) {
            uint64_t dt = tnow - dev->last_change_ms;
            if (debounce_ms == 0 || dt >= (uint64_t)debounce_ms) {
                dev->last_raw = raw_now;
                dev->last_change_ms = tnow;
                do_cb = (cb != NULL);
                me = value_to_event(dev, raw_now);
            }
        }
        pthread_mutex_unlock(&dev->lock);

        if (do_cb) {
            cb(dev, me, cb_args);
        }
#endif
    }

    return NULL;
}

/* ---- API implementation ---- */

struct misc_dev *misc_io_alloc(enum misc_type type, enum misc_dir dir, void *hw_ctx)
{
    const struct misc_gpiod_ctx *ctx = (const struct misc_gpiod_ctx *)hw_ctx;
    if (!ctx || !ctx->chip_name)
        return NULL;

    struct misc_dev *dev = (struct misc_dev *)calloc(1, sizeof(*dev));
    if (!dev)
        return NULL;

    dev->type = type;
    dev->dir = dir;
    dev->active_logic = MISC_ACTIVE_HIGH;
    dev->debounce_ms = 0;
    dev->last_raw = -1;
    dev->last_change_ms = 0;

    pthread_mutex_init(&dev->lock, NULL);

#if defined(LIBGPIOD_V2)
    dev->offset = ctx->line_offset;
    if (ctx->chip_name[0] == '/') {
        dev->chip = gpiod_chip_open(ctx->chip_name);
    } else {
        dev->chip = gpiod_chip_open_lookup(ctx->chip_name);
    }
#else
    dev->chip = gpiod_chip_open_by_name(ctx->chip_name);
#endif
    if (!dev->chip) {
        misc_io_free(dev);
        return NULL;
    }

#if !defined(LIBGPIOD_V2)
    dev->line = gpiod_chip_get_line(dev->chip, ctx->line_offset);
    if (!dev->line) {
        misc_io_free(dev);
        return NULL;
    }
#endif

    /* Request line direction now for get/set usability.
     * For input we request event-capable input once.
     */
    if (dir == MISC_DIR_INPUT) {
        if (request_line_input_events(dev, ctx->consumer) != 0) {
            misc_io_free(dev);
            return NULL;
        }
    } else {
        /* default output to inactive */
        int inactive_raw = (dev->active_logic == MISC_ACTIVE_HIGH) ? 0 : 1;
        if (request_line_output(dev, ctx->consumer, inactive_raw) != 0) {
            misc_io_free(dev);
            return NULL;
        }
    }

    return dev;
}

void misc_io_config(struct misc_dev *dev, enum misc_logic active_logic, uint16_t debounce_ms)
{
    if (!dev)
        return;

    pthread_mutex_lock(&dev->lock);
    dev->active_logic = active_logic;
    dev->debounce_ms = debounce_ms;
    pthread_mutex_unlock(&dev->lock);
}

int misc_io_set(struct misc_dev *dev, bool active)
{
#if defined(LIBGPIOD_V2)
    if (!dev || !dev->req)
#else
    if (!dev || !dev->line)
#endif
        return -EINVAL;
    if (dev->dir != MISC_DIR_OUTPUT)
        return -EPERM;

    pthread_mutex_lock(&dev->lock);
    enum misc_logic logic = dev->active_logic;
    pthread_mutex_unlock(&dev->lock);

    int raw = 0;
    if (logic == MISC_ACTIVE_HIGH) {
        raw = active ? 1 : 0;
    } else {
        raw = active ? 0 : 1;
    }

    int ret = write_raw_value(dev, raw);
    if (ret < 0)
        return ret;

    return 0;
}

int misc_io_get(struct misc_dev *dev)
{
#if defined(LIBGPIOD_V2)
    if (!dev || !dev->req)
#else
    if (!dev || !dev->line)
#endif
        return -EINVAL;

    int raw = read_raw_value(dev);
    if (raw < 0)
        return raw;

    /* Return active/inactive as 1/0 */
    pthread_mutex_lock(&dev->lock);
    enum misc_logic logic = dev->active_logic;
    pthread_mutex_unlock(&dev->lock);

    int active_raw = (logic == MISC_ACTIVE_HIGH) ? 1 : 0;
    return (raw == active_raw) ? 1 : 0;
}

void misc_io_trigger(struct misc_dev *dev, misc_cb_t cb, void *args)
{
    if (!dev)
        return;

    pthread_mutex_lock(&dev->lock);
    dev->cb = cb;
    dev->cb_args = args;

    /* Only meaningful for input lines */
    if (dev->dir != MISC_DIR_INPUT) {
        pthread_mutex_unlock(&dev->lock);
        return;
    }

    if (!dev->thread_started) {
        dev->running = 1;
        dev->thread_started = 1;

        /* Start background thread */
        if (pthread_create(&dev->th, NULL, trigger_thread_fn, dev) != 0) {
            dev->running = 0;
            dev->thread_started = 0;
            pthread_mutex_unlock(&dev->lock);
            return;
        }
    }
    pthread_mutex_unlock(&dev->lock);
}

void misc_io_free(struct misc_dev *dev)
{
    if (!dev)
        return;

    /* Stop thread if running */
    pthread_mutex_lock(&dev->lock);
    int join_needed = dev->thread_started;
    dev->running = 0;
    pthread_mutex_unlock(&dev->lock);

    if (join_needed) {
        pthread_join(dev->th, NULL);
    }

#if defined(LIBGPIOD_V2)
    if (dev->req) {
        gpiod_line_request_release(dev->req);
        dev->req = NULL;
    }
    if (dev->rcfg) {
        gpiod_request_config_free(dev->rcfg);
        dev->rcfg = NULL;
    }
    if (dev->lcfg) {
        gpiod_line_config_free(dev->lcfg);
        dev->lcfg = NULL;
    }
    if (dev->settings) {
        gpiod_line_settings_free(dev->settings);
        dev->settings = NULL;
    }
#else
    if (dev->line) {
        gpiod_line_release(dev->line);
        dev->line = NULL;
    }
#endif
    if (dev->chip) {
        gpiod_chip_close(dev->chip);
        dev->chip = NULL;
    }

    pthread_mutex_destroy(&dev->lock);
    free(dev);
}

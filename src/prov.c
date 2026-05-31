/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/input/input.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(prov, LOG_LEVEL_DBG);

#include "pn532.h"

static const struct device *const longpress_dev = DEVICE_DT_GET(DT_PATH(longpress));
static const struct device *const buttons_dev = DEVICE_DT_GET(DT_PATH(buttons));

static void prov_thread_entry(void *p1, void *p2, void *p3);
static void prov_longpress_cb(struct input_event *evt, void *user_data);
static int prov_cmd_fake_longpress(const struct shell *sh, size_t argc, char **argv);

K_THREAD_DEFINE(prov_thread, 4096, prov_thread_entry, NULL, NULL, NULL, 8, 0, 0);
INPUT_CALLBACK_DEFINE(longpress_dev, prov_longpress_cb, NULL);
SHELL_CMD_REGISTER(prov_fake_longpress, NULL, "Fake long press", prov_cmd_fake_longpress);

static void prov_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	LOG_INF("Provisioning thread started...");

	while (1) {
		k_msleep(2000);
	}
}

static void prov_longpress_cb(struct input_event *evt, void *user_data)
{
	LOG_DBG("type=%d code=%d value=%d sync=%d", evt->type, evt->code, evt->value, evt->sync);

	if (!evt->sync) {
		return;
	}
	if (evt->type != INPUT_EV_KEY) {
		return;
	}
	if (evt->value == 0) {
		return; /* ignore key-up from longpress driver */
	}

	switch (evt->code) {
	case INPUT_BTN_0:
		LOG_INF("Long press: button %i", evt->code);
		break;
	}
}

/*
 * Fake the sync'd longpress event that prov_longpress_cb expects
 */
static int prov_cmd_fake_longpress(const struct shell *sh, size_t argc, char **argv)
{
	/* Press */
	input_report_key(buttons_dev, INPUT_BTN_0, 1, true, K_NO_WAIT);
	/* Wait */
	k_msleep(2100);
	/* Release */
	input_report_key(buttons_dev, INPUT_BTN_0, 0, true, K_NO_WAIT);

	return 0;
}

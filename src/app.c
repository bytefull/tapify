/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/shell/shell.h>
#include <zephyr/input/input.h>
#include <zephyr/settings/settings.h>
#include <zephyr/smf.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#define APP_SETTINGS_KEY              "app"
#define APP_SETTINGS_PROV_KEY         "app/provisioned"
#define APP_DEFAULT_PROVISIONED_VALUE 0

struct smf_event {
	uint32_t id;
};

enum app_events {
	EVENT_BUTTON_LONG_PRESS,
	EVENT_BUTTON_SHORT_PRESS,
	EVENT_MAX,
};

struct state_machine {
	struct smf_ctx ctx;
	struct smf_event event;
} app_state_machine;

enum app_states {
	STATE_IDLE,
	STATE_PROV,
	STATE_AUTH,
	STATE_ERROR,
};

static void app_thread_entry(void *p1, void *p2, void *p3);
static int app_settings_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg);
static int app_settings_export(int (*cb)(const char *key, const void *data, size_t data_len));
static void app_long_press_cb(struct input_event *evt, void *user_data);
static int app_cmd_fake_provision(const struct shell *sh, size_t argc, char **argv);
static int app_cmd_fake_longpress(const struct shell *sh, size_t argc, char **argv);

static void state_idle_entry(void *obj);
static enum smf_state_result state_idle_run(void *obj);
static void state_idle_exit(void *obj);

static void state_prov_entry(void *obj);
static enum smf_state_result state_prov_run(void *obj);
static void state_prov_exit(void *obj);

static void state_auth_entry(void *obj);
static enum smf_state_result state_auth_run(void *obj);
static void state_auth_exit(void *obj);

static void state_error_entry(void *obj);
static enum smf_state_result state_error_run(void *obj);
static void state_error_exit(void *obj);

static uint8_t device_is_provisioned = APP_DEFAULT_PROVISIONED_VALUE;
static const struct device *const longpress_dev = DEVICE_DT_GET(DT_PATH(longpress));
static const struct device *const prov_buttons_dev = DEVICE_DT_GET(DT_ALIAS(prov_buttons));
static const struct smf_state app_states[] = {
	[STATE_IDLE] =
		SMF_CREATE_STATE(state_idle_entry, state_idle_run, state_idle_exit, NULL, NULL),
	[STATE_PROV] =
		SMF_CREATE_STATE(state_prov_entry, state_prov_run, state_prov_exit, NULL, NULL),
	[STATE_AUTH] =
		SMF_CREATE_STATE(state_auth_entry, state_auth_run, state_auth_exit, NULL, NULL),
	[STATE_ERROR] =
		SMF_CREATE_STATE(state_error_entry, state_error_run, state_error_exit, NULL, NULL),
};

K_THREAD_DEFINE(app_thread, 4096, app_thread_entry, NULL, NULL, NULL, 8, 0, 0);
K_MSGQ_DEFINE(app_msgq, sizeof(struct smf_event), 8, 1);
INPUT_CALLBACK_DEFINE(longpress_dev, app_long_press_cb, NULL);
SHELL_CMD_REGISTER(app_fake_provision, NULL, "Fake device provisioning", app_cmd_fake_provision);
SHELL_CMD_REGISTER(app_fake_longpress, NULL, "Fake long press", app_cmd_fake_longpress);
SETTINGS_STATIC_HANDLER_DEFINE(app, APP_SETTINGS_KEY, NULL, app_settings_set, NULL,
			       app_settings_export);

static void app_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	int ret;

	LOG_INF("Application thread started...");
	smf_set_initial(SMF_CTX(&app_state_machine), &app_states[STATE_IDLE]);

	ret = settings_subsys_init();
	if (ret) {
		LOG_ERR("settings subsys initialization: fail (err %d)", ret);
		return;
	}
	ret = settings_load_subtree(APP_SETTINGS_KEY);
	if (ret) {
		LOG_ERR("settings load: fail (err %d)", ret);
		return;
	}

	if (device_is_provisioned) {
		LOG_INF("Device is provisioned");
		smf_set_state(SMF_CTX(&app_state_machine), &app_states[STATE_AUTH]);
	} else {
		LOG_WRN("Device is not provisioned");
		smf_set_state(SMF_CTX(&app_state_machine), &app_states[STATE_PROV]);
	}

	while (1) {
		int ret = k_msgq_get(&app_msgq, &app_state_machine.event, K_FOREVER);
		if (ret != 0) {
			LOG_ERR("Waiting for event failed, code %d", ret);
			k_msleep(1000);
			continue;
		}
		/* Run state machine with given message */
		ret = smf_run_state(SMF_CTX(&app_state_machine));
		if (ret) {
			/* State machine terminates if a non-zero value is returned */
			LOG_INF("%s terminating state machine thread", __func__);
			break;
		}
	}
}

static int app_settings_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg)
{
	if (strcmp(key, "provisioned") == 0) {
		if (len != sizeof(device_is_provisioned)) {
			LOG_WRN("size mismatch for key '%s': stored %zu vs expected %zu", key, len,
				sizeof(device_is_provisioned));
			return -EINVAL;
		}
		ssize_t ret =
			read_cb(cb_arg, &device_is_provisioned, sizeof(device_is_provisioned));
		if (ret < 0) {
			/* propagate error */
			return (int)ret;
		}
		if (ret == 0) {
			/* key was deleted */
			LOG_WRN("key '%s' deleted, resetting to default value: %d", key,
				APP_DEFAULT_PROVISIONED_VALUE);
			device_is_provisioned = APP_DEFAULT_PROVISIONED_VALUE;
		}
		/* success */
		return 0;
	}
	return -ENOENT;
}

static int app_settings_export(int (*cb)(const char *key, const void *data, size_t data_len))
{
	return cb(APP_SETTINGS_PROV_KEY, &device_is_provisioned, sizeof(device_is_provisioned));
}

static void app_long_press_cb(struct input_event *evt, void *user_data)
{
	ARG_UNUSED(user_data);

	LOG_DBG("type=%d code=%d value=%d sync=%d", evt->type, evt->code, evt->value, evt->sync);

	if (!evt->sync) {
		return;
	}
	if (evt->type != INPUT_EV_KEY) {
		return;
	}
	switch (evt->code) {
	case INPUT_BTN_0:
		LOG_INF("Long press: button %i", evt->code);
		struct smf_event event = {
			.id = EVENT_BUTTON_LONG_PRESS,
		};
		k_msgq_put(&app_msgq, &event, K_NO_WAIT);
		break;
	}
}

static int app_cmd_fake_provision(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(sh);

	int ret;

	if (argc != 2) {
		shell_error(sh, "usage: app_fake_provision <0|1>");
		return -EINVAL;
	}

	if (strcmp(argv[1], "0") == 0) {
		device_is_provisioned = false;
	} else if (strcmp(argv[1], "1") == 0) {
		device_is_provisioned = true;
	} else {
		shell_error(sh, "invalid argument: %s, expected 0 or 1", argv[1]);
		return -EINVAL;
	}

	ret = settings_save_one(APP_SETTINGS_PROV_KEY, &device_is_provisioned,
				sizeof(device_is_provisioned));
	if (ret) {
		LOG_ERR("settings save: fail (err %d)", ret);
		return ret;
	}

	LOG_INF("device_is_provisioned set to %d and persisted", device_is_provisioned);
	smf_set_state(SMF_CTX(&app_state_machine),
		      device_is_provisioned ? &app_states[STATE_AUTH] : &app_states[STATE_PROV]);

	return 0;
}

static int app_cmd_fake_longpress(const struct shell *sh, size_t argc, char **argv)
{
	ARG_UNUSED(sh);
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	/* Press */
	input_report_key(prov_buttons_dev, INPUT_BTN_0, 1, true, K_NO_WAIT);
	/* Wait */
	k_msleep(2100);
	/* Release */
	input_report_key(prov_buttons_dev, INPUT_BTN_0, 0, true, K_NO_WAIT);

	return 0;
}

static void state_idle_entry(void *obj)
{
	LOG_INF("%s", __func__);
}

static enum smf_state_result state_idle_run(void *obj)
{
	return SMF_EVENT_PROPAGATE;
}

static void state_idle_exit(void *obj)
{
	LOG_INF("%s", __func__);
}

static void state_prov_entry(void *obj)
{
	LOG_INF("%s", __func__);
}

static enum smf_state_result state_prov_run(void *obj)
{
	return SMF_EVENT_PROPAGATE;
}

static void state_prov_exit(void *obj)
{
	LOG_INF("%s", __func__);
}

static void state_auth_entry(void *obj)
{
	LOG_INF("%s", __func__);
}

static enum smf_state_result state_auth_run(void *obj)
{
	enum smf_state_result ret;
	struct state_machine *object = (struct state_machine *)obj;

	switch (object->event.id) {
	case EVENT_BUTTON_LONG_PRESS:
		LOG_INF("EVENT_BUTTON_LONG_PRESS");
		smf_set_state(SMF_CTX(object), &app_states[STATE_PROV]);
		ret = SMF_EVENT_HANDLED;
		break;
	case EVENT_BUTTON_SHORT_PRESS:
		LOG_INF("EVENT_BUTTON_SHORT_PRESS");
		ret = SMF_EVENT_HANDLED;
		break;
	default:
		ret = SMF_EVENT_PROPAGATE;
		break;
	}
	return ret;
}

static void state_auth_exit(void *obj)
{
	LOG_INF("%s", __func__);
}

static void state_error_entry(void *obj)
{
	LOG_INF("%s", __func__);
}

static enum smf_state_result state_error_run(void *obj)
{
	return SMF_EVENT_PROPAGATE;
}

static void state_error_exit(void *obj)
{
	LOG_INF("%s", __func__);
}

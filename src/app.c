/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/settings/settings.h>
#include <zephyr/smf.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#define APP_SETTINGS_KEY              "app"
#define APP_SETTINGS_PROV_KEY         "app/provisioned"
#define APP_DEFAULT_PROVISIONED_VALUE 0

static void app_thread_entry(void *p1, void *p2, void *p3);
static int app_settings_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg);
static int app_settings_export(int (*cb)(const char *key, const void *data, size_t data_len));
static int app_cmd_fake_provision(const struct shell *sh, size_t argc, char **argv);

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

struct app_event {
	uint32_t id;
};

K_THREAD_DEFINE(app_thread, 4096, app_thread_entry, NULL, NULL, NULL, 8, 0, 0);
K_MSGQ_DEFINE(app_msgq, sizeof(struct app_event), 8, 1);
SHELL_CMD_REGISTER(app_fake_provision, NULL, "Fake device provisioning", app_cmd_fake_provision);
SETTINGS_STATIC_HANDLER_DEFINE(app, APP_SETTINGS_KEY, NULL, app_settings_set, NULL,
			       app_settings_export);

static uint8_t device_is_provisioned = APP_DEFAULT_PROVISIONED_VALUE;

enum app_events {
	EVENT_BUTTON_LONG_PRESS,
	EVENT_BUTTON_SHORT_PRESS,
	EVENT_MAX,
};

struct state_machine {
	struct smf_ctx ctx;
	struct app_event event;
} app_state_machine;

enum app_states {
	STATE_IDLE,
	STATE_PROV,
	STATE_AUTH,
	STATE_ERROR,
};

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
		/* TODO: Listen for NFC targets and go into authentication state... */
	} else {
		LOG_WRN("Device is not provisioned");
		/* TODO: Listen for NFC targets and go into provisioning state... */
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

	return 0;
}

static void state_idle_entry(void *o)
{
}
static enum smf_state_result state_idle_run(void *obj)
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
static void state_idle_exit(void *o)
{
}

static void state_prov_entry(void *o)
{
}
static enum smf_state_result state_prov_run(void *o)
{
	return SMF_EVENT_PROPAGATE;
}
static void state_prov_exit(void *o)
{
}

static void state_auth_entry(void *o)
{
}
static enum smf_state_result state_auth_run(void *o)
{
	return SMF_EVENT_PROPAGATE;
}
static void state_auth_exit(void *o)
{
}

static void state_error_entry(void *o)
{
}
static enum smf_state_result state_error_run(void *o)
{
	return SMF_EVENT_PROPAGATE;
}
static void state_error_exit(void *o)
{
}

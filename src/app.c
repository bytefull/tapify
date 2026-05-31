/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include <zephyr/settings/settings.h>
#include <zephyr/storage/flash_map.h>
#ifdef CONFIG_SETTINGS_FILE
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#endif /* CONFIG_SETTINGS_FILE */
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_DBG);

#include "pn532.h"

#define STORAGE_PARTITION_ID PARTITION_ID(storage_partition)

#define APP_SETTINGS_KEY              "app"
#define APP_SETTINGS_PROV_KEY         "app/provisioned"
#define APP_DEFAULT_PROVISIONED_VALUE 0

static void app_thread_entry(void *p1, void *p2, void *p3);
static int app_settings_set(const char *key, size_t len, settings_read_cb read_cb, void *cb_arg);
static int app_settings_export(int (*cb)(const char *key, const void *data, size_t data_len));
static int app_cmd_fake_provision(const struct shell *sh, size_t argc, char **argv);

K_THREAD_DEFINE(app_thread, 4096, app_thread_entry, NULL, NULL, NULL, 8, 0, 0);
SHELL_CMD_REGISTER(app_fake_provision, NULL, "Fake device provisioning", app_cmd_fake_provision);
SETTINGS_STATIC_HANDLER_DEFINE(app, APP_SETTINGS_KEY, NULL, app_settings_set, NULL,
			       app_settings_export);

static uint8_t device_is_provisioned = false;

static void app_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	int ret;

	LOG_INF("Application thread started...");
#ifdef CONFIG_SETTINGS_FILE
	FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(cstorage);

	/* mounting info */
	static struct fs_mount_t littlefs_mnt = {.type = FS_LITTLEFS,
						 .fs_data = &cstorage,
						 .storage_dev = (void *)STORAGE_PARTITION_ID,
						 .mnt_point = "/settings"};

	ret = fs_mount(&littlefs_mnt);
	if (ret != 0) {
		LOG_ERR("mounting littlefs error: [%d]", ret);
		return;
	}
	LOG_INF("FS initialized: OK");
#endif /* CONFIG_SETTINGS_FILE */

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
	} else {
		LOG_WRN("Device is not provisioned");
	}

	while (1) {
		k_msleep(1000);
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

/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief native_sim buzzer driver (Zephyr-side / top layer).
 *
 * Implements the Zephyr buzzer driver API by delegating all sound output
 * to the host-side bottom layer (buzzer_native_sim_bottom.c).
 *
 * The driver intentionally mirrors the structure of buzzer_gpio.c:
 *   - k_work_delayable for the timed auto-stop
 *   - k_mutex protecting all mutable state
 *   - volume stored as a percentage (0–BUZZER_VOLUME_MAX)
 *
 * A NATIVE_TASK hook runs the bottom-layer init before PRE_KERNEL_1 so the
 * host resources are ready when buzzer_native_sim_init() is called by the
 * Zephyr device model.
 */

#define DT_DRV_COMPAT native_sim_buzzer

#include <zephyr/drivers/buzzer.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <nsi_tracing.h>
#include "posix_native_task.h"
#include "buzzer_native_sim_bottom.h"

LOG_MODULE_REGISTER(buzzer_native_sim, CONFIG_BUZZER_LOG_LEVEL);

/* Default frequency used by buzzer_beep() — middle A, universally recognisable */
#define NATIVE_SIM_BUZZER_DEFAULT_FREQ_HZ 440U
#define NATIVE_SIM_BUZZER_DEFAULT_VOLUME  50U

struct buzzer_native_sim_data {
	const struct device      *dev;
	struct k_work_delayable   stop_work;
	struct k_mutex            lock;
	uint8_t                   volume_percent;
};

/* --------------------------------------------------------------------------
 * Internal helpers
 * -------------------------------------------------------------------------- */

static void buzzer_native_sim_stop_work(struct k_work *work)
{
	struct k_work_delayable *dw =
		k_work_delayable_from_work(work);
	struct buzzer_native_sim_data *data =
		CONTAINER_OF(dw, struct buzzer_native_sim_data, stop_work);
	int ret;

	ARG_UNUSED(data); /* used only for the CONTAINER_OF resolution above */

	ret = buzzer_native_sim_bottom_stop();
	if (ret < 0) {
		LOG_ERR("Auto-stop failed (%d)", ret);
	}
}

static void buzzer_native_sim_cancel_stop(struct buzzer_native_sim_data *data)
{
	struct k_work_sync sync;

	k_work_cancel_delayable_sync(&data->stop_work, &sync);
}

/* --------------------------------------------------------------------------
 * Driver API implementation
 * -------------------------------------------------------------------------- */

static int buzzer_native_sim_tone(const struct device *dev,
				  uint32_t freq_hz,
				  uint32_t duration_ms)
{
	struct buzzer_native_sim_data *data = dev->data;
	bool active;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);

	buzzer_native_sim_cancel_stop(data);

	active = (freq_hz != BUZZER_FREQ_REST) && (data->volume_percent != 0U);

	ret = buzzer_native_sim_bottom_tone(active ? freq_hz : 0U,
					    data->volume_percent);
	if (ret < 0) {
		k_mutex_unlock(&data->lock);
		return ret;
	}

	if (active && duration_ms != BUZZER_DURATION_FOREVER) {
		ret = k_work_schedule(&data->stop_work, K_MSEC(duration_ms));
		if (ret < 0) {
			/*
			 * Scheduling failed — silence immediately so we don't
			 * leave the buzzer on indefinitely.
			 */
			int silence_ret = buzzer_native_sim_bottom_stop();

			if (silence_ret < 0) {
				LOG_ERR("Failed to silence after schedule error (%d)",
					silence_ret);
			}
			k_mutex_unlock(&data->lock);
			return ret;
		}
	}

	k_mutex_unlock(&data->lock);
	return 0;
}

static int buzzer_native_sim_set_volume(const struct device *dev, uint8_t percent)
{
	struct buzzer_native_sim_data *data = dev->data;
	int ret = 0;

	if (percent > BUZZER_VOLUME_MAX) {
		return -EINVAL;
	}

	k_mutex_lock(&data->lock, K_FOREVER);

	if (percent == 0U) {
		data->volume_percent = 0U;
		buzzer_native_sim_cancel_stop(data);
		ret = buzzer_native_sim_bottom_stop();
	} else {
		/*
		 * Unlike GPIO-driven active buzzers, the evdev PC speaker
		 * back-end does accept a frequency, so we store the real
		 * percentage. The bottom layer logs it but cannot act on it
		 * today; storing it means a future back-end upgrade is free.
		 */
		data->volume_percent = percent;
	}

	k_mutex_unlock(&data->lock);
	return ret;
}

static int buzzer_native_sim_beep(const struct device *dev, uint32_t duration_ms)
{
	return buzzer_native_sim_tone(dev, NATIVE_SIM_BUZZER_DEFAULT_FREQ_HZ,
				      duration_ms);
}

static int buzzer_native_sim_stop(const struct device *dev)
{
	struct buzzer_native_sim_data *data = dev->data;
	int ret;

	k_mutex_lock(&data->lock, K_FOREVER);
	buzzer_native_sim_cancel_stop(data);
	ret = buzzer_native_sim_bottom_stop();
	k_mutex_unlock(&data->lock);

	return ret;
}

/* --------------------------------------------------------------------------
 * Device init
 * -------------------------------------------------------------------------- */

static int buzzer_native_sim_init(const struct device *dev)
{
	struct buzzer_native_sim_data *data = dev->data;

	data->dev            = dev;
	data->volume_percent = NATIVE_SIM_BUZZER_DEFAULT_VOLUME;

	k_mutex_init(&data->lock);
	k_work_init_delayable(&data->stop_work, buzzer_native_sim_stop_work);

	LOG_INF("native_sim buzzer ready");
	return 0;
}

/* --------------------------------------------------------------------------
 * Driver API table
 * -------------------------------------------------------------------------- */

static DEVICE_API(buzzer, buzzer_native_sim_api) = {
	.tone       = buzzer_native_sim_tone,
	.set_volume = buzzer_native_sim_set_volume,
	.beep       = buzzer_native_sim_beep,
	.stop       = buzzer_native_sim_stop,
};

/* --------------------------------------------------------------------------
 * Device tree instantiation
 * -------------------------------------------------------------------------- */

#define BUZZER_NATIVE_SIM_INIT(inst)						\
	static struct buzzer_native_sim_data buzzer_native_sim_data_##inst;	\
										\
	DEVICE_DT_INST_DEFINE(inst,						\
			      buzzer_native_sim_init,				\
			      NULL,						\
			      &buzzer_native_sim_data_##inst,			\
			      NULL,						\
			      POST_KERNEL,					\
			      CONFIG_BUZZER_INIT_PRIORITY,			\
			      &buzzer_native_sim_api);

DT_INST_FOREACH_STATUS_OKAY(BUZZER_NATIVE_SIM_INIT)

/* --------------------------------------------------------------------------
 * NATIVE_TASK hooks — run on the host side, outside the Zephyr scheduler
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialise the host sound back-end before any Zephyr driver init.
 *
 * PRE_BOOT_1 runs before PRE_KERNEL_1, so the bottom layer is ready by the
 * time buzzer_native_sim_init() is called by the device model.
 */
static void buzzer_native_sim_host_init(void)
{
	int ret = buzzer_native_sim_bottom_init();

	if (ret < 0) {
		nsi_print_warning("buzzer_native_sim: host init failed (%d), "
				  "buzzer output will be silent\n", ret);
	}
}

/**
 * @brief Release host resources on simulator exit.
 */
static void buzzer_native_sim_host_cleanup(void)
{
	buzzer_native_sim_bottom_cleanup();
}

NATIVE_TASK(buzzer_native_sim_host_init,    PRE_BOOT_1, 50);
NATIVE_TASK(buzzer_native_sim_host_cleanup, ON_EXIT,    50);

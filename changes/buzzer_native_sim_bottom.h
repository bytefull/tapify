/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Host-side (bottom) interface for the native_sim buzzer driver.
 *
 * Functions declared here run in the POSIX/host domain and must never
 * call Zephyr kernel APIs directly. They are called from the Zephyr-side
 * (top) driver through the NSI host-trampoline boundary.
 */

#ifndef BUZZER_NATIVE_SIM_BOTTOM_H
#define BUZZER_NATIVE_SIM_BOTTOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the host sound back-end.
 *
 * Called once during driver init. On Linux this attempts to open the PC
 * speaker evdev node; on other POSIX hosts it falls back to the terminal
 * bell. Always succeeds (degraded operation is acceptable).
 *
 * @return 0 on success, negative errno-compatible value on hard failure.
 */
int buzzer_native_sim_bottom_init(void);

/**
 * @brief Start emitting a tone at @p freq_hz on the host.
 *
 * Passing 0 for @p freq_hz is equivalent to calling
 * buzzer_native_sim_bottom_stop().
 *
 * Volume is advisory: back-ends that cannot control volume (terminal bell,
 * PC speaker without a mixer) log the requested value and ignore it.
 *
 * @param freq_hz       Desired frequency in Hz, or 0 for silence.
 * @param volume_percent Volume hint, 0–100.
 *
 * @return 0 on success, negative errno-compatible value on failure.
 */
int buzzer_native_sim_bottom_tone(uint32_t freq_hz, uint8_t volume_percent);

/**
 * @brief Stop any tone currently playing on the host.
 *
 * Idempotent.
 *
 * @return 0 on success, negative errno-compatible value on failure.
 */
int buzzer_native_sim_bottom_stop(void);

/**
 * @brief Clean up host resources acquired during init.
 *
 * Called on simulator exit via NATIVE_TASK(…, ON_EXIT, …).
 */
void buzzer_native_sim_bottom_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* BUZZER_NATIVE_SIM_BOTTOM_H */

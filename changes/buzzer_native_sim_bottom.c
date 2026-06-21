/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Host-side (bottom) implementation for the native_sim buzzer driver.
 *
 * This file is compiled as part of the POSIX/host layer. It must not call
 * any Zephyr kernel API. Two sound back-ends are provided:
 *
 *   1. Linux evdev PC speaker  (/dev/input/by-path/platform-pcspkr-event-spkr)
 *      Sends EV_SND / SND_TONE events. Works without root on most desktop
 *      distributions (the user must be in the "input" group, or udev grants
 *      access automatically for seat members). Gives real frequency control.
 *
 *   2. ALSA speaker-test fallback
 *      Spawns speaker-test (part of alsa-utils) as a child process.
 *      Works inside devcontainers when /dev/snd is passed through via
 *      --device=/dev/snd in devcontainer.json. Gives real frequency control
 *      with no third-party dependencies beyond alsa-utils.
 */

#include "nsi_tracing.h"

#include "buzzer_native_sim_bottom.h"

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/wait.h>

extern char **environ;

/* --------------------------------------------------------------------------
 * Linux evdev back-end (compiled only on Linux)
 * -------------------------------------------------------------------------- */
#ifdef __linux__

#include <linux/input.h>

#define PCSPKR_PATH "/dev/input/by-path/platform-pcspkr-event-spkr"

static int  pcspkr_fd = -1;
static bool use_evdev = false;

static int pcspkr_write_event(uint16_t type, uint16_t code, int32_t value)
{
	struct input_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.type  = type;
	ev.code  = code;
	ev.value = value;

	ssize_t n = write(pcspkr_fd, &ev, sizeof(ev));

	if (n != sizeof(ev)) {
		nsi_print_warning("buzzer_native_sim: evdev write failed (%d)\n", errno);
		return -errno;
	}
	return 0;
}

static int pcspkr_set_freq(uint32_t freq_hz)
{
	int ret;

	ret = pcspkr_write_event(EV_SND, SND_TONE, (int32_t)freq_hz);
	if (ret < 0) {
		return ret;
	}
	return pcspkr_write_event(EV_SYN, SYN_REPORT, 0);
}

#endif /* __linux__ */

/* --------------------------------------------------------------------------
 * ALSA speaker-test back-end
 * -------------------------------------------------------------------------- */

/* PID of the currently running speaker-test child, -1 if none */
static pid_t speaker_test_pid = -1;

static void speaker_test_stop(void)
{
	if (speaker_test_pid > 0) {
		kill(speaker_test_pid, SIGTERM);
		waitpid(speaker_test_pid, NULL, 0);
		speaker_test_pid = -1;
	}
}

static int speaker_test_play(uint32_t freq_hz)
{
	char freq_str[16];

	snprintf(freq_str, sizeof(freq_str), "%u", freq_hz);

	/*
	 * -t sine  : sine wave
	 * -f <hz>  : frequency
	 * -l 1     : one loop (plays for one period then exits;
	 *             the Zephyr top layer schedules a stop_work anyway)
	 */
	char *argv[] = {
		"speaker-test", "-t", "sine",
		"-f", freq_str,
		"-l", "1",
		NULL
	};

	int ret = posix_spawnp(&speaker_test_pid, "speaker-test",
			       NULL, NULL, argv, environ);

	if (ret != 0) {
		nsi_print_warning("buzzer_native_sim: failed to spawn speaker-test (%d); "
				  "is alsa-utils installed?\n", ret);
		speaker_test_pid = -1;
		return -ENOENT;
	}

	return 0;
}

/* --------------------------------------------------------------------------
 * Public bottom API
 * -------------------------------------------------------------------------- */

int buzzer_native_sim_bottom_init(void)
{
#ifdef __linux__
	pcspkr_fd = open(PCSPKR_PATH, O_WRONLY);
	if (pcspkr_fd < 0) {
		nsi_print_trace(
			"buzzer_native_sim: cannot open %s (%s); "
			"falling back to ALSA speaker-test\n",
			PCSPKR_PATH, strerror(errno));
		use_evdev = false;
	} else {
		nsi_print_trace("buzzer_native_sim: using PC speaker via evdev (%s)\n",
				PCSPKR_PATH);
		use_evdev = true;
	}
#else
	nsi_print_trace("buzzer_native_sim: using ALSA speaker-test (non-Linux host)\n");
#endif
	return 0;
}

int buzzer_native_sim_bottom_tone(uint32_t freq_hz, uint8_t volume_percent)
{
	/* Always stop any previous speaker-test child before starting a new one */
	speaker_test_stop();

	if (freq_hz != 0U) {
		nsi_print_trace("buzzer_native_sim: tone %u Hz, volume %u%%\n",
				freq_hz, volume_percent);
	}

#ifdef __linux__
	if (use_evdev) {
		return pcspkr_set_freq(freq_hz);
	}
#endif

	if (freq_hz == 0U || volume_percent == 0U) {
		return 0;
	}

	return speaker_test_play(freq_hz);
}

int buzzer_native_sim_bottom_stop(void)
{
	speaker_test_stop();

#ifdef __linux__
	if (use_evdev) {
		return pcspkr_set_freq(0);
	}
#endif

	return 0;
}

void buzzer_native_sim_bottom_cleanup(void)
{
	speaker_test_stop();

#ifdef __linux__
	if (pcspkr_fd >= 0) {
		pcspkr_set_freq(0);
		close(pcspkr_fd);
		pcspkr_fd = -1;
		use_evdev  = false;
	}
#endif
}
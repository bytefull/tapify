# Copyright (c) 2026 Carlo Caione <ccaione@baylibre.com>
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_BUZZER_GPIO buzzer_gpio.c)
zephyr_library_sources_ifdef(CONFIG_BUZZER_PWM  buzzer_pwm.c)

if(CONFIG_BUZZER_NATIVE_SIM)
  zephyr_library_sources(buzzer_native_sim.c)
  target_sources(native_simulator INTERFACE buzzer_native_sim_bottom.c)
endif()

#!/bin/bash
set -e

west init -l application
west update
west zephyr-export

sudo /opt/python/venv/bin/pip install \
    -r /workdir/dependencies/zephyr/scripts/requirements.txt

sudo apt install -y alsa-utils

sudo npm install -g \
    purgecss \
    html-minifier-terser \
    clean-css-cli \
    terser
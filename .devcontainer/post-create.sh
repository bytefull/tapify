#!/bin/bash
set -e

sudo /opt/python/venv/bin/python3 -m pip install --upgrade pip

west init -l application
west update
west zephyr-export

sudo /opt/python/venv/bin/pip install \
    -r /workdir/dependencies/zephyr/scripts/requirements.txt

sudo npm install -g \
    purgecss \
    html-minifier-terser \
    clean-css-cli \
    terser
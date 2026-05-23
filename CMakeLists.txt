# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(zephyr_nfc_hce)

if (CONFIG_NETWORKING)
  include(${ZEPHYR_BASE}/samples/net/common/common.cmake)
endif()

target_sources(app PRIVATE
    src/main.c
    src/web.c
    src/auth.c
)

zephyr_linker_sources(SECTIONS sections-rom.ld)
zephyr_linker_section(NAME http_resource_desc_web_service KVMA RAM_REGION GROUP RODATA_REGION)

set(gen_dir ${ZEPHYR_BINARY_DIR}/include/generated/)

find_program(PURGECSS_EXECUTABLE purgecss)

if(NOT PURGECSS_EXECUTABLE)
    message(FATAL_ERROR "purgecss not found")
endif()

set(PURGED_BOOTSTRAP ${CMAKE_BINARY_DIR}/bootstrap.purged.min.css)

add_custom_command(
    OUTPUT ${PURGED_BOOTSTRAP}
    COMMAND ${PURGECSS_EXECUTABLE}
        --css ${CMAKE_CURRENT_SOURCE_DIR}/web/bootstrap.min.css
        --content
            ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
            ${CMAKE_CURRENT_SOURCE_DIR}/web/script.js
        --safelist bg-danger text-white
        --output ${PURGED_BOOTSTRAP}
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/web/bootstrap.min.css
        ${CMAKE_CURRENT_SOURCE_DIR}/web/index.html
        ${CMAKE_CURRENT_SOURCE_DIR}/web/script.js
)

set(source_file_index web/index.html)
generate_inc_file_for_target(app ${source_file_index} ${gen_dir}/index.html.gz.inc --gzip)

set(source_file_bootstrap_purged_min_css ${PURGED_BOOTSTRAP})
generate_inc_file_for_target(app ${source_file_bootstrap_purged_min_css} ${gen_dir}/bootstrap.purged.min.css.gz.inc --gzip)

set(source_file_style web/style.css)
generate_inc_file_for_target(app ${source_file_style} ${gen_dir}/style.css.gz.inc --gzip)

set(source_file_script web/script.js)
generate_inc_file_for_target(app ${source_file_script} ${gen_dir}/script.js.gz.inc --gzip)

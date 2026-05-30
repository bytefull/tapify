/*
 * Copyright (c) 2026 Bayrem Gharsellaoui
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/net/http/server.h>
#include <zephyr/net/http/service.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(web, LOG_LEVEL_DBG);

#include "net_sample_common.h"
#ifdef CONFIG_USB_DEVICE_STACK_NEXT
#include "sample_usbd.h"
#endif /* CONFIG_USB_DEVICE_STACK_NEXT */

static void web_thread_entry(void *p1, void *p2, void *p3);
K_THREAD_DEFINE(web_thread, 4096, web_thread_entry, NULL, NULL, NULL, 8, 0, 0);

static bool led_on = false;
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static const uint16_t http_service_port = 8080;
HTTP_SERVICE_DEFINE(web_service, "0.0.0.0", &http_service_port, 8, 10, NULL, NULL, NULL);

static int led_request_handler(struct http_client_ctx *client, enum http_transaction_status status,
			       const struct http_request_ctx *request_ctx,
			       struct http_response_ctx *response_ctx, void *user_data);

/* *************************** favicon.ico *************************** */
static const uint8_t favicon[] = {0};
static struct http_resource_detail_static favicon_resource_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "image/x-icon",
		},
	.static_data = favicon,
	.static_data_len = sizeof(favicon),
};
HTTP_RESOURCE_DEFINE(favicon_resource, web_service, "/favicon.ico", &favicon_resource_detail);

/* *************************** index.html *************************** */
static const uint8_t index_html_gz[] = {
#include "index.html.gz.inc"
};
static struct http_resource_detail_static index_html_gz_resource_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "text/html",
		},
	.static_data = index_html_gz,
	.static_data_len = sizeof(index_html_gz),
};
HTTP_RESOURCE_DEFINE(index_html_gz_resource, web_service, "/", &index_html_gz_resource_detail);

/* *************************** bootstrap.purged.min.css *************************** */
static const uint8_t bootstrap_purged_min_css_gz[] = {
#include "bootstrap.purged.min.css.gz.inc"
};
static struct http_resource_detail_static bootstrap_purged_min_css_gz_resource_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "text/css",
		},
	.static_data = bootstrap_purged_min_css_gz,
	.static_data_len = sizeof(bootstrap_purged_min_css_gz),
};
HTTP_RESOURCE_DEFINE(bootstrap_purged_min_css_gz_resource, web_service, "/bootstrap.purged.min.css",
		     &bootstrap_purged_min_css_gz_resource_detail);

/* *************************** style.css *************************** */
static const uint8_t style_css_gz[] = {
#include "style.css.gz.inc"
};
static struct http_resource_detail_static style_css_gz_resource_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "text/css",
		},
	.static_data = style_css_gz,
	.static_data_len = sizeof(style_css_gz),
};
HTTP_RESOURCE_DEFINE(style_css_gz_resource, web_service, "/style.css",
		     &style_css_gz_resource_detail);

/* *************************** script.js *************************** */
static const uint8_t script_js_gz[] = {
#include "script.js.gz.inc"
};
static struct http_resource_detail_static script_js_gz_resource_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_encoding = "gzip",
			.content_type = "application/javascript",
		},
	.static_data = script_js_gz,
	.static_data_len = sizeof(script_js_gz),
};
HTTP_RESOURCE_DEFINE(script_js_gz_resource, web_service, "/script.js",
		     &script_js_gz_resource_detail);

/* *************************** json data *************************** */
static struct http_resource_detail_static json_message_resource_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_STATIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET),
			.content_type = "application/json",
		},
	.static_data = "{\"message\":\"HTTP server is running\"}",
	.static_data_len = sizeof("{\"message\":\"HTTP server is running\"}") - 1,
};
HTTP_RESOURCE_DEFINE(json_message_resource, web_service, "/data", &json_message_resource_detail);

/* *************************** led *************************** */
static struct http_resource_detail_dynamic led_resource_detail = {
	.common =
		{
			.type = HTTP_RESOURCE_TYPE_DYNAMIC,
			.bitmask_of_supported_http_methods = BIT(HTTP_GET) | BIT(HTTP_POST),
			.content_type = "application/json",
		},
	.cb = led_request_handler,
	.user_data = NULL,
};
HTTP_RESOURCE_DEFINE(led_resource, web_service, "/led", &led_resource_detail);
static int led_request_handler(struct http_client_ctx *client, enum http_transaction_status status,
			       const struct http_request_ctx *request_ctx,
			       struct http_response_ctx *response_ctx, void *user_data)
{
	ARG_UNUSED(user_data);
	static size_t cursor = 0;

	if (client->method == HTTP_GET) {
		static char response[32] = {'\0'};

		snprintk(response, sizeof(response), "{\"led\":%s}", led_on ? "true" : "false");
		response_ctx->status = 200;
		response_ctx->body = response;
		response_ctx->body_len = strlen(response);
		response_ctx->final_chunk = true;
	} else if (client->method == HTTP_POST) {
		static char post_payload[64] = {'\0'};

		/* Reset on abort/complete */
		if ((status == HTTP_SERVER_TRANSACTION_ABORTED) ||
			(status == HTTP_SERVER_TRANSACTION_COMPLETE)) {
			cursor = 0;
			return 0;
		}

		/* Prevent overflow */
		if ((cursor + request_ctx->data_len) >= sizeof(post_payload)) {
			cursor = 0;
			return -ENOMEM;
		}

		/* Append chunk */
		memcpy(post_payload + cursor, request_ctx->data, request_ctx->data_len);

		cursor += request_ctx->data_len;

		/* Null terminate */
		post_payload[cursor] = '\0';

		/* Wait until final chunk */
		if (status == HTTP_SERVER_REQUEST_DATA_FINAL) {
			LOG_DBG("Received JSON: %s", post_payload);
			if (strcmp(post_payload, "{\"led\":true}") == 0) {
				LOG_INF("LED ON");
				gpio_pin_set_dt(&led, 1);
				led_on = true;
			} else if (strcmp(post_payload, "{\"led\":false}") == 0) {
				LOG_INF("LED OFF");
				gpio_pin_set_dt(&led, 0);
				led_on = false;
			} else {
				LOG_WRN("Unknown payload");
			}
			cursor = 0;
		}
	}

	return 0;
}

static void web_thread_entry(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	/* Initialize USB Device */
	IF_ENABLED(CONFIG_USB_DEVICE_STACK_NEXT, ({
		struct usbd_context *sample_usbd = sample_usbd_init_device(NULL);
		if (sample_usbd == NULL) {
			LOG_ERR("Failed to initialize USB device");
			return;
		}
		if (usbd_enable(sample_usbd) < 0) {
			LOG_ERR("Failed to enable USB device");
			return;
		}
	}));

	/* Block thread to wait for getting an IP address from the network */
	wait_for_network();
	LOG_INF("Network is ready");

	/* Start HTTP server in a background thread */
	http_server_start();
	LOG_INF("Starting HTTP server...");

	/* Configure LED GPIO */
	if (!gpio_is_ready_dt(&led)) {
		return;
	}
	if (gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE) < 0) {
		return;
	}

	while (1) {
		k_msleep(2000);
	}
}

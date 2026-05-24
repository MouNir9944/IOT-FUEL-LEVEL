/*
 * Copyright (c) 2024 <SFM Technologies>
 */

#ifndef SETUP_MODE_BUTTON_H
#define SETUP_MODE_BUTTON_H

#include "Commun_Lib.h"
#include "nvs_Manager.h"

#define setup_mode_button GPIO_NUM_0

#define NVS_NAMESPACE    "storage"
#define KEY_NAME         "mode"
#define Gateway_mode_nvs "1"   /* default: operation mode */

static const char *TAG_BUTTON = "button";
static QueueHandle_t gpio_evt_queue2 = NULL;

void gpio_button_mode_init(gpio_num_t gpio_switchmode);

/* Forward declarations — bodies are in wifi_manager.h (included after this file) */
void wifi_enable_ap(void);
void wifi_stop_sta_if_not_connected(void);

/* ── ISR ─────────────────────────────────────────────────────────────────── */
static void IRAM_ATTR gpio_isr_handler2(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue2, &gpio_num, NULL);
}

/* ── Button task ─────────────────────────────────────────────────────────── */
static void gpio_task(void *arg)
{
    uint32_t io_num;
    TickType_t last_press_tick = 0;

    for (;;) {
        if (!xQueueReceive(gpio_evt_queue2, &io_num, portMAX_DELAY)) continue;
        if (io_num != setup_mode_button) continue;

        /* ── Debounce: ignore bounces within 2 s of the last accepted press ── */
        TickType_t now = xTaskGetTickCount();
        if ((now - last_press_tick) < pdMS_TO_TICKS(2000)) continue;
        last_press_tick = now;

        ESP_LOGI(TAG_BUTTON, "Setup button pressed — enabling AP mode");

        /* Stop STA retry loop only if not currently connected.
         * If already connected, STA is left untouched so the link stays up. */
        wifi_stop_sta_if_not_connected();

        wifi_enable_ap();
    }
}

/* ── Init ────────────────────────────────────────────────────────────────── */
void gpio_button_mode_init(gpio_num_t gpio_switchmode)
{
    gpio_config_t io_conf = {
        .intr_type    = GPIO_INTR_POSEDGE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << gpio_switchmode),
        .pull_down_en = 0,
        .pull_up_en   = 1,
    };
    gpio_config(&io_conf);

    gpio_evt_queue2 = xQueueCreate(4, sizeof(uint32_t));
    xTaskCreate(gpio_task, "btn_task", 4096, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(gpio_switchmode, gpio_isr_handler2,
                         (void *)gpio_switchmode);

    ESP_LOGI(TAG_BUTTON, "Button on GPIO%d ready", gpio_switchmode);
}

#endif /* SETUP_MODE_BUTTON_H */

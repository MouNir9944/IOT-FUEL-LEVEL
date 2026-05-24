#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "driver/gpio.h"
#include "esp_log.h"

/* ─────────────────────────────────────────────────────────────────────────
 * Status LED assignments
 * ─────────────────────────────────────────────────────────────────────────
 *  GPIO 32 - WiFi   ON = station associated with router
 *  GPIO 33 - MQTT   ON = broker connection active
 *  GPIO 26 - Auto   ON = control_mode == "auto" (plan scheduler running)
 *  GPIO 27 - Valve  ON = valve is opening (motor energized open direction)
 *
 * All LEDs are active-HIGH (GPIO HIGH = LED illuminated).
 * ─────────────────────────────────────────────────────────────────────────
 */
#define LED_PIN_WIFI   GPIO_NUM_32
#define LED_PIN_MQTT   GPIO_NUM_33
#define LED_PIN_AUTO   GPIO_NUM_26
#define LED_PIN_VALVE  GPIO_NUM_27

/* Named set macros - pass 1 (or any non-zero) for ON, 0 for OFF */
#define led_wifi_set(on)   gpio_set_level(LED_PIN_WIFI,  (on) ? 1 : 0)
#define led_mqtt_set(on)   gpio_set_level(LED_PIN_MQTT,  (on) ? 1 : 0)
#define led_auto_set(on)   gpio_set_level(LED_PIN_AUTO,  (on) ? 1 : 0)
#define led_valve_set(on)  gpio_set_level(LED_PIN_VALVE, (on) ? 1 : 0)

/**
 * Configure all four LED GPIOs as push-pull outputs and drive them LOW.
 * Call once at the very start of app_main().
 */
static inline void led_init(void)
{
    static const gpio_num_t pins[] = {
        LED_PIN_WIFI, LED_PIN_MQTT, LED_PIN_AUTO, LED_PIN_VALVE
    };

    for (int i = 0; i < (int)(sizeof(pins) / sizeof(pins[0])); i++) {
        gpio_config_t cfg = {
            .pin_bit_mask = 1ULL << pins[i],
            .mode         = GPIO_MODE_OUTPUT,
            .pull_up_en   = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_DISABLE,
        };
        gpio_config(&cfg);
        gpio_set_level(pins[i], 0);
    }

    ESP_LOGI("LED", "Initialized - WiFi=GPIO%d  MQTT=GPIO%d  Auto=GPIO%d  Valve=GPIO%d",
             LED_PIN_WIFI, LED_PIN_MQTT, LED_PIN_AUTO, LED_PIN_VALVE);
}

#endif /* LED_MANAGER_H */

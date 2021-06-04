#include <stddef.h>
#include <stdio.h>

#include "btstack_port_esp32.h"
#include "btstack_run_loop.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "pg9021_mapping.h"

#define BUTTON_CONNECT_PIN 17

static int64_t button_pressed_last_time = 0;

static xQueueHandle button_evt_queue = NULL;

extern int btstack_main(int argc, const char* argv[]);

extern void connect_gamepad();
extern void set_gamepad_mac(const char* mac);
extern void set_gamepad_action_callback(gamepad_handler_t callback);
extern void btstack_run_loop_freertos_execute_code_on_main_thread(
    void (*fn)(void* arg), void* arg);

static void print_action(char* event, int32_t value, bool analog);
static void on_gamepad_action(uint16_t page, uint16_t event, int32_t value);

static void print_action(char* event, int32_t value, bool analog) {
  if (analog) {
    printf("%s %d\n", event, value);
  } else {
    if (value == 0) {
      printf("%s released\n\n", event);
    } else if (value == 1) {
      printf("%s pressed\n", event);
    }
  }
}

static void on_gamepad_action(uint16_t page, uint16_t event, int32_t value) {
  switch (page) {
    case PAGE_KEYBOARD_BUTTONS:
      switch (event) {
          // D-pad
        case KB_DPAD_UP:
          print_action("KB_DPAD_UP", value, false);
          break;
        case KB_DPAD_DOWN:
          print_action("KB_DPAD_DOWN", value, false);
          break;
        case KB_DPAD_RIGHT:
          print_action("KB_DPAD_RIGHT", value, false);
          break;
        case KB_DPAD_LEFT:
          print_action("KB_DPAD_LEFT", value, false);
          break;
          // Buttons are the main gamepad buttons
        case KB_BUTTON_A:
          print_action("KB_BUTTON_A", value, false);
          break;
        case KB_BUTTON_B:
          print_action("KB_BUTTON_B", value, false);
          break;
        case KB_BUTTON_X:
          print_action("KB_BUTTON_X", value, false);
          break;
        case KB_BUTTON_Y:
          print_action("KB_BUTTON_Y", value, false);
          break;
        case KB_BUTTON_SHOULDER_L:
          print_action("KB_BUTTON_SHOULDER_L", value, false);
          break;
        case KB_BUTTON_SHOULDER_R:
          print_action("KB_BUTTON_SHOULDER_R", value, false);
          break;
        case KB_BUTTON_TRIGGER_L:
          print_action("KB_BUTTON_TRIGGER_L", value, false);
          break;
        case KB_BUTTON_TRIGGER_R:
          print_action("KB_BUTTON_TRIGGER_R", value, false);
          break;
        case KB_BUTTON_THUMB_L:
          print_action("KB_BUTTON_THUMB_L", value, false);
          break;
        case KB_BUTTON_THUMB_R:
          print_action("KB_BUTTON_THUMB_R", value, false);
          break;
          // Misc main buttons
        case KB_MISC_BUTTON_START:
          print_action("KB_MISC_BUTTON_START", value, false);
          break;
        case KB_MISC_BUTTON_SELECT:
          print_action("KB_MISC_BUTTON_SELECT", value, false);
          break;
          // Thumb
        case KB_THUMB_L_UP:
          print_action("KB_THUMB_L_UP", value, false);
          break;
        case KB_THUMB_L_DOWN:
          print_action("KB_THUMB_L_DOWN", value, false);
          break;
        case KB_THUMB_L_RIGHT:
          print_action("KB_THUMB_L_RIGHT", value, false);
          break;
        case KB_THUMB_L_LEFT:
          print_action("KB_THUMB_L_LEFT", value, false);
          break;
        case KB_THUMB_R_UP:
          print_action("KB_THUMB_R_UP", value, false);
          break;
        case KB_THUMB_R_DOWN:
          print_action("KB_THUMB_R_DOWN", value, false);
          break;
        case KB_THUMB_R_RIGHT:
          print_action("KB_THUMB_R_RIGHT", value, false);
          break;
        case KB_THUMB_R_LEFT:
          print_action("KB_THUMB_R_LEFT", value, false);
          break;
        default:
          break;
      }
      break;

    case PAGE_MISC_ADDITIONAL_BUTTONS:
      switch (event) {
        case MISC_BUTTON_HOME:
          print_action("MISC_BUTTON_HOME", value, false);
          break;
        case MISC_BUTTON_MINUS:
          print_action("MISC_BUTTON_MINUS", value, false);
          break;
        case MISC_BUTTON_PREV:
          print_action("MISC_BUTTON_PREV", value, false);
          break;
        case MISC_BUTTON_PLAY:
          print_action("MISC_BUTTON_PLAY", value, false);
          break;
        case MISC_BUTTON_NEXT:
          print_action("MISC_BUTTON_NEXT", value, false);
          break;
        case MISC_BUTTON_PLUS:
          print_action("MISC_BUTTON_PLUS", value, false);
          break;
        default:
          break;
      }
      break;

    case PAGE_GAMEPAD_BUTTONS:
      switch (event) {
        // Buttons are the main gamepad buttons
        case GP_BUTTON_A:
          print_action("GP_BUTTON_A", value, false);
          break;
        case GP_BUTTON_B:
          print_action("GP_BUTTON_B", value, false);
          break;
        case GP_BUTTON_C:
          print_action("GP_BUTTON_C", value, false);
          break;
        case GP_BUTTON_X:
          print_action("GP_BUTTON_X", value, false);
          break;
        case GP_BUTTON_Y:
          print_action("GP_BUTTON_Y", value, false);
          break;
        case GP_BUTTON_Z:
          print_action("GP_BUTTON_Z", value, false);
          break;
        case GP_BUTTON_SHOULDER_L:
          print_action("GP_BUTTON_SHOULDER_L", value, false);
          break;
        case GP_BUTTON_SHOULDER_R:
          print_action("GP_BUTTON_SHOULDER_R", value, false);
          break;
        case GP_BUTTON_TRIGGER_L:
          print_action("GP_BUTTON_TRIGGER_L", value, false);
          break;
        case GP_BUTTON_TRIGGER_R:
          print_action("GP_BUTTON_TRIGGER_R", value, false);
          break;
        case GP_BUTTON_UNKNOWN:
          print_action("GP_BUTTON_UNKNOWN", value, true);
          break;
        case GP_BUTTON_THUMB_L:
          print_action("GP_BUTTON_THUMB_L", value, false);
          break;
        case GP_BUTTON_THUMB_R:
          print_action("GP_BUTTON_THUMB_R", value, false);
          break;
          // Misc main buttons
        case GP_MISC_BUTTON_START:
          print_action("GP_MISC_BUTTON_START", value, false);
          break;
        case GP_MISC_BUTTON_SELECT:
          print_action("GP_MISC_BUTTON_SELECT", value, false);
          break;
        default:
          break;
      }
      break;

    case PAGE_GAMEPAD_DPAD_THUMB:
      switch (event) {
        // D-pad
        case GP_DPAD_UP:
          print_action("GP_DPAD_UP", value, false);
          break;
        case GP_DPAD_DOWN:
          print_action("GP_DPAD_DOWN", value, false);
          break;
        case GP_DPAD_RIGHT:
          print_action("GP_DPAD_RIGHT", value, false);
          break;
        case GP_DPAD_LEFT:
          print_action("GP_DPAD_LEFT", value, false);
          break;
        // Thumb axis
        case GP_THUMB_L_X:
          print_action("GP_THUMB_L_X", value, true);
          break;
        case GP_THUMB_L_Y:
          print_action("GP_THUMB_L_Y", value, true);
          break;
        case GP_THUMB_R_X:
          print_action("GP_THUMB_R_X", value, true);
          break;
        case GP_THUMB_R_Y:
          print_action("GP_THUMB_R_Y", value, true);
          break;
        default:
          break;
      }
      break;

    default:
      break;
  }
}

static void IRAM_ATTR button_handler(void* arg) {
  uint32_t button_pin = (uint32_t)arg;
  xQueueSendFromISR(button_evt_queue, &button_pin, NULL);
}

static void gpio_task(void* arg) {
  uint32_t io_num;
  int64_t end_time;
  for (;;) {
    if (xQueueReceive(button_evt_queue, &io_num, portMAX_DELAY)) {
      end_time = esp_timer_get_time();
      if (end_time - button_pressed_last_time > 700000) {
        button_pressed_last_time = end_time;
        printf("Connect button pressed\n");
        void* ptr;
        btstack_run_loop_freertos_execute_code_on_main_thread(&connect_gamepad,
                                                              &ptr);
      }
    }
  }
}

static void setup_connect_button() {
  gpio_config_t button_conf;
  button_conf.intr_type = GPIO_PIN_INTR_POSEDGE;  // LOW -> HIGH
  button_conf.pin_bit_mask = (1ULL << BUTTON_CONNECT_PIN);
  button_conf.mode = GPIO_MODE_INPUT;
  button_conf.pull_down_en = 0;
  button_conf.pull_up_en = 1;
  gpio_config(&button_conf);

  // Create a queue to handle gpio event from isr
  button_evt_queue = xQueueCreate(10, sizeof(uint32_t));
  xTaskCreate(gpio_task, "gpio_task", 2048, NULL, 10, NULL);

  // Install gpio isr service
  gpio_install_isr_service(0);
  // Hook isr handler for button connect pin
  gpio_isr_handler_add(BUTTON_CONNECT_PIN, button_handler,
                       (void*)BUTTON_CONNECT_PIN);
}

int app_main(void) {

  // MAC address your iPega PG-9021
  set_gamepad_mac("00:90:E1:D1:9D:96");

  // Prepare connect button
  setup_connect_button();

  // Set function to receive gamepad signals
  set_gamepad_action_callback(&on_gamepad_action);

  // Configure BTstack for ESP32 VHCI Controller
  btstack_init();

  // Setup btstack
  btstack_main(0, NULL);

  // Enter run loop (forever)
  btstack_run_loop_execute();

  return 0;
}

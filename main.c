/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/rand.h"
#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"

static bool enabled = true;

void button_task(void);
void hid_task(void);

/*------------- MAIN -------------*/
int main(void)
{
  board_init();

  // init device stack on configured roothub port
  tud_init(BOARD_TUD_RHPORT);

  if (board_init_after_tusb)
  {
    board_init_after_tusb();
  }

  board_led_write(enabled);

  while (1)
  {
    tud_task(); // tinyusb device task
    button_task();

    hid_task();
  }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
  (void)remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(uint8_t report_id)
{
  // skip if hid is not ready yet
  if (!tud_hid_ready())
    return;

  if (report_id == REPORT_ID_MOUSE)
  {
    uint32_t r32 = get_rand_32();

    int8_t x_delta = r32 & 0xFF; // get 8 bits for x
    int8_t y_delta = (r32 >> 8) & 0xFF; // get next 8 bits for y

    // no button, right + down, no scroll, no pan
    tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, x_delta, y_delta, 0, 0);
  }
}

static uint32_t read_debounced_button(void)
{
  // Read the button state and debounce it
  static uint32_t last_button_state = 0;
  static uint32_t last_debounce_time = 0;
  const uint32_t debounce_delay = 50; // ms

  uint32_t current_button_state = board_button_read();

  if (current_button_state != last_button_state)
  {
    last_debounce_time = board_millis();
  }

  if ((board_millis() - last_debounce_time) > debounce_delay)
  {
    last_button_state = current_button_state;
  }

  return last_button_state;
}

void button_task(void)
{
  // Read the debounced button state
  uint32_t btn = read_debounced_button();

  // If the button is pressed, toggle the enabled state
  if (btn)
  {
    enabled = 1 - enabled;
    board_led_write(enabled);
  }
}

// Every 10ms, we will sent 1 report for each HID profile (keyboard, mouse etc ..)
// tud_hid_report_complete_cb() is used to send the next report after previous one is complete
void hid_task(void)
{
  // Poll every 10ms
  const uint32_t wake_interval_ms = 10;
  static uint32_t wake_start_ms = 0;

  if (board_millis() - wake_start_ms < wake_interval_ms)
    return; // not enough time

  wake_start_ms += wake_interval_ms;

  // Remote wakeup
  if (tud_suspended())
  {
    // Wake up host if we are in suspend mode
    // and REMOTE_WAKEUP feature is enabled by host
    tud_remote_wakeup();
  }

  const uint32_t mouse_interval_ms = 5000;
  static uint32_t mouse_start_ms = 0;

  if (board_millis() - mouse_start_ms < mouse_interval_ms)
    return; // not enough time

  mouse_start_ms += mouse_interval_ms;

  if (!enabled) return;

  send_hid_report(REPORT_ID_MOUSE);
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
  (void)instance;
  (void)len;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
  // TODO not Implemented
  (void)instance;
  (void)report_id;
  (void)report_type;
  (void)buffer;
  (void)reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
  (void)instance;
  (void)report_id;
  (void)report_type;
}

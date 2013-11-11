#pragma once

#include <pebble.h>

void set_timer();
void handle_tap();
void set_bpm(int bpm_in);
void update_bpm_text();
void recalculate_bpm();
uint32_t now();
int get_magnitude_sq(AccelData accel);
void accel_data_handler_single(AccelData accel_data);
void accel_data_handler(AccelData *accel_data, uint32_t num_samples);
void accel_data_null_handler(AccelData *accel_data, uint32_t num_samples);
void timer_callback_sampling();
void timer_callback_vibing();

void select_click_handler(ClickRecognizerRef recognizer, void *context);
/* void select_long_click_handler(ClickRecognizerRef recognizer, void *context); */
void up_click_handler(ClickRecognizerRef recognizer, void *context);
void down_click_handler(ClickRecognizerRef recognizer, void *context);
void click_config_provider(void *context);
void window_load(Window *window);
void window_unload(Window *window);
void init(void);
void deinit(void);
int main(void);

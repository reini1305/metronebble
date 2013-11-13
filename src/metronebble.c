// Metronebble - a Metronome for Pebble 2.0


#include <pebble.h>
#include "metronebble.h"

static Window *window;
static TextLayer *bpm_layer;
static TextLayer *status_layer;
static AppTimer *timer;

int last_mag = 0;
bool invert = false;
uint32_t last_tap = 0;

// the maximum ratio (*10) of last accel magnitude to new accel magnitude ratio,
// to count as a tap
#define DIFF_RATIO_FACTOR 10
#define DIFF_RATIO_MAX 12 // i.e. 1.2 times the previous magnitude
// minimum delay between registering tap events
#define MIN_TAP_TIME 150
#define TIMER_FREQUENCY_SAMPLING 4 // 250Hz

// how many taps to sample to get the bpm
#define NUM_TAPS 10

uint32_t taps[NUM_TAPS];
int tap_index = 0;
int bpm = 0;
uint32_t bpm_start_time = 0;
int bpm_tick_count = 0;

uint32_t last_timer = 0;
int timer_updates = 0;

#define BPM_TEXT_LENGTH 20
static char bpm_text[BPM_TEXT_LENGTH] = "";

typedef enum STATE {
  STATE_IDLE,
  STATE_SAMPLING,
  STATE_VIBING
} STATE;
STATE state = STATE_IDLE;


static const uint32_t const tick_pattern_segments[] = { 50 };
VibePattern tick_pattern = {
  .durations = tick_pattern_segments,
  .num_segments = ARRAY_LENGTH(tick_pattern_segments),
};


void set_timer() {
  app_timer_cancel(timer);
  if (state == STATE_IDLE) return;
  else if (state == STATE_SAMPLING) timer = app_timer_register(TIMER_FREQUENCY_SAMPLING, timer_callback_sampling, NULL);
  else if (state == STATE_VIBING) {
    /* figure out how far bpm has drifted */
    /* get millis*1000 for a bpm tick (*1000 for better accuracy) */
    uint32_t millis_gap = (1000l * (1000l * 60)) / bpm;
    /* next tick is start_time + (millis_gap * tick_count) */
    uint32_t next_tick = bpm_start_time + ((millis_gap * bpm_tick_count) / 1000l);
    int timer_delay = (int)(next_tick - now());
    /* compensate for drifting below now, allow the timer to catchup */
    if (timer_delay < 0) {
      timer_delay = 0;
    }

    timer = app_timer_register(timer_delay, timer_callback_vibing, NULL);
    bpm_tick_count++;
  }
}

// handle a tap, in any direction
void handle_tap() { 
  text_layer_set_background_color(bpm_layer, invert ? GColorBlack : GColorWhite);
  text_layer_set_text_color(bpm_layer, invert ? GColorWhite : GColorBlack);
  invert = !invert;

  tap_index = (tap_index + 1) % NUM_TAPS;
  taps[tap_index] = now();
  recalculate_bpm();
  update_bpm_text();
}

void set_bpm(int bpm_in) {
  bpm = bpm_in;
  bpm_start_time = now();
  bpm_tick_count = 1;
}

void update_bpm_text() {
  snprintf(bpm_text, BPM_TEXT_LENGTH, "BPM: %d", bpm);
  text_layer_set_text(bpm_layer, bpm_text);
}

void recalculate_bpm() {
  uint32_t latest_tap = taps[tap_index];
  uint32_t first_tap = taps[(tap_index + 1) % NUM_TAPS];
  // time difference for 10 taps is actually the total time elapsed /
  // 9 (as the last tap has no time)
  uint32_t average_tap_time = (1000l * (latest_tap - first_tap)) / (NUM_TAPS - 1);
  
  set_bpm((int)((1000l * 1000l * 60) / average_tap_time));
}

// get the current time in millis
uint32_t now() {
  time_t s = 0;
  uint16_t ms = 0;
  time_ms(&s, &ms);
  return s * 1000l + ms;
}

// get the square of the magnitude of this acceleration data -
// saves doing a sqrt
int get_magnitude_sq(AccelData accel) {
  return accel.x * accel.x +
    accel.y + accel.y +
    accel.z * accel.z;
}

void accel_data_handler_single(AccelData accel) {
  int mag = get_magnitude_sq(accel);
  if (last_mag != 0) {
    // figure out the ratio difference between the last magnitude of
    // accel data and the current one; if it's bigger than
    // DIFF_RATIO_MAX, then this is probably a tap (or acceleration
    // has changed sufficiently strongly and fast to register it as
    // one)
    int diff = last_mag * DIFF_RATIO_FACTOR / mag;
    if (diff > DIFF_RATIO_MAX && now() - last_tap > MIN_TAP_TIME) {
      handle_tap();
      last_tap = now();
    }
  }
  last_mag = mag;
}

// handle acceleration data collection - this method just discovers taps in a slightly
// more reliable way than the system call for accel_tap_service_subscribe
void accel_data_handler(AccelData *accel_data, uint32_t num_samples) {
  for (uint32_t i=1; i<num_samples; ++i) {
    accel_data_handler_single(accel_data[i]);
  }
}

void accel_data_null_handler(AccelData *accel_data, uint32_t num_samples) {
  // nothing
}


void timer_callback_sampling() {

  AccelData accel = {0, 0, 0};
  accel_service_peek(&accel);
  accel_data_handler_single(accel);
  
  timer_updates++;
  last_timer = now();

  set_timer();
}

void timer_callback_vibing() {
  set_timer();

  /* vibes_short_pulse(); */
  vibes_enqueue_custom_pattern(tick_pattern);
}



void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  switch (state) {
    case STATE_IDLE:
      state = STATE_SAMPLING;
      text_layer_set_text(status_layer, "Sampling");
      break;
    case STATE_SAMPLING:
      state = STATE_VIBING;
      // reset the bpm counter state
      set_bpm(bpm);
      text_layer_set_text(status_layer, "Ticking");
      break;
    case STATE_VIBING:
      state = STATE_IDLE;
      text_layer_set_text(status_layer, "Idle");
      break;      
  }
  set_timer();
}

/* void select_long_click_handler(ClickRecognizerRef recognizer, void *context) { */
/*   //   toggle_sampling(); */
/* } */

void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  set_bpm(bpm+1);
  update_bpm_text();
}

void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  set_bpm(bpm-1);
  update_bpm_text();
}

void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  /* window_long_click_subscribe(BUTTON_ID_SELECT, select_long_click_handler); */
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  bpm_layer = text_layer_create((GRect) { .origin = { 0, 72 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(bpm_layer, "Sample/Buzz -->");
  text_layer_set_text_alignment(bpm_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(bpm_layer));

  status_layer = text_layer_create((GRect) { .origin = { 0, bounds.size.h - 20 }, .size = { bounds.size.w, 20 } });
  text_layer_set_text(status_layer, "Idle");
  text_layer_set_text_alignment(status_layer, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(status_layer));
}

void window_unload(Window *window) {
  text_layer_destroy(status_layer);
  text_layer_destroy(bpm_layer);
}

void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
      .load = window_load,
	.unload = window_unload,
	});
  const bool animated = true;
  window_stack_push(window, animated);

  /* accel_service_set_sampling_rate(ACCEL_SAMPLING_100HZ); */
  /* accel_data_service_subscribe(10, accel_data_handler); */
  accel_data_service_subscribe(0, accel_data_null_handler);

  for (int i=0; i<NUM_TAPS; ++i) {
    taps[i] = 0;
  }

  set_timer();
}

void deinit(void) {
  accel_data_service_unsubscribe();
  window_destroy(window);
  set_timer();
}

int main(void) {
  init();

  //  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}

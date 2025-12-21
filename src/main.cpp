#include <Arduino.h>
#include <lvgl.h>
#include "lgfx_setup.h"
#include <math.h>

/* ===================== MAX6675 (bit-bang) ===================== */
/* IMPORTANT: ne pas utiliser SPI.begin()/SPI.beginTransaction() sinon écran noir (bus partagé avec TFT) */

static constexpr int PIN_MAX_CS   = 22;  // 
static constexpr int PIN_MAX_SCK  = 21;  // 
static constexpr int PIN_MAX_SO   = 35;  // 

static void max6675_begin()
{
  pinMode(PIN_MAX_CS, OUTPUT);
  digitalWrite(PIN_MAX_CS, HIGH);

  pinMode(PIN_MAX_SCK, OUTPUT);
  digitalWrite(PIN_MAX_SCK, LOW);

  pinMode(PIN_MAX_SO, INPUT);
}

static uint16_t max6675_read_raw()
{
  digitalWrite(PIN_MAX_CS, LOW);
  delayMicroseconds(2);

  uint16_t v = 0;
  for(int i = 0; i < 16; i++) {
    digitalWrite(PIN_MAX_SCK, HIGH);
    delayMicroseconds(2);
    v = (uint16_t)((v << 1) | (digitalRead(PIN_MAX_SO) ? 1 : 0));
    digitalWrite(PIN_MAX_SCK, LOW);
    delayMicroseconds(2);
  }

  digitalWrite(PIN_MAX_CS, HIGH);
  return v;
}

static float max6675_raw_to_c(uint16_t v)
{
  if (v & 0x0004) return NAN;  // thermocouple open
  v >>= 3;
  return v * 0.25f;
}

/* ===================== GT911 reset (0x5D) ===================== */
static void gt911_reset_0x5D()
{
  constexpr int TP_INT = 21;
  constexpr int TP_RST = 25;

  pinMode(TP_INT, OUTPUT);
  pinMode(TP_RST, OUTPUT);

  digitalWrite(TP_INT, HIGH); // => 0x5D
  delay(2);

  digitalWrite(TP_RST, LOW);
  delay(20);
  digitalWrite(TP_RST, HIGH);
  delay(120);

  pinMode(TP_INT, INPUT);
}

/* ===================== LVGL display glue ===================== */
static lv_display_t * g_disp = nullptr;

static void lv_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
  LV_UNUSED(disp);

  const int32_t w = (area->x2 - area->x1 + 1);
  const int32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels(reinterpret_cast<const lgfx::rgb565_t *>(px_map), (uint32_t)w * (uint32_t)h);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

static void lv_touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data)
{
  LV_UNUSED(indev);

  uint16_t x, y;
  bool touched = tft.getTouch(&x, &y);

  if(touched) {
    if(x >= 320) x = 319;
    if(y >= 480) y = 479;
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

/* ===================== UI: jauge + bouton ===================== */
static lv_obj_t * scale_obj = nullptr;
static lv_obj_t * needle_img = nullptr;
static lv_obj_t * temp_label = nullptr;
static lv_obj_t * motor_btn = nullptr;
static lv_obj_t * motor_btn_label = nullptr;
static bool motor_on = false;
static int32_t current_temp = 0;
#ifndef MOTOR_PIN
  #define MOTOR_PIN 26
#endif

static void motor_set()
{
  digitalWrite(MOTOR_PIN, HIGH);
  delay(10);
  digitalWrite(MOTOR_PIN, LOW);
  delay(10);
  
}

static void update_needle(int32_t value_celsius)
{
  if(value_celsius < 0) value_celsius = 0;
  if(value_celsius > 500) value_celsius = 500;

  // 0°C = -135°, 250°C = 0°, 500°C = +135°
  float frac = value_celsius / 500.0f;
  int16_t angle_deg = (int16_t)(-135.0f + frac * 270.0f);

  // LVGL en 0.1°
  lv_obj_set_style_transform_angle(needle_img, angle_deg * 10, 0);
}

static void set_temperature(int32_t value_celsius)
{
  if(value_celsius < 0) value_celsius = 0;
  if(value_celsius > 500) value_celsius = 500;

  update_needle(value_celsius);

  char buf[16];
  snprintf(buf, sizeof(buf), "%ld°C", (long)value_celsius);
  lv_label_set_text(temp_label, buf);
}

static void motor_btn_event_cb(lv_event_t * e)
{
  if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

  Serial.println("motor_btn_event_cb...");
  motor_on = !motor_on;

  lv_obj_set_style_bg_color(motor_btn,
    motor_on ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED),
    0);
}

static void create_ui()
{
  // Cadran
  scale_obj = lv_scale_create(lv_screen_active());
  lv_obj_set_size(scale_obj, 240, 240);
  lv_obj_align(scale_obj, LV_ALIGN_TOP_MID, 0, 50); // décalé vers le bas

  lv_scale_set_mode(scale_obj, LV_SCALE_MODE_ROUND_OUTER);
  lv_scale_set_label_show(scale_obj, true);

  lv_scale_set_range(scale_obj, 0, 500);
  lv_scale_set_total_tick_count(scale_obj, 51);  // tick tous les 10°C
  lv_scale_set_major_tick_every(scale_obj, 5);   // majeur tous les 50°C

  lv_scale_set_angle_range(scale_obj, 270);
  lv_scale_set_rotation(scale_obj, 135);

  // Style ticks majeurs
  static lv_style_t major_style;
  lv_style_init(&major_style);
  lv_style_set_line_color(&major_style, lv_color_black());
  lv_style_set_line_width(&major_style, 2);
  lv_style_set_width(&major_style, 10);
  lv_obj_add_style(scale_obj, &major_style, LV_PART_INDICATOR);

  // Style ticks mineurs
  static lv_style_t minor_style;
  lv_style_init(&minor_style);
  lv_style_set_line_color(&minor_style, lv_palette_main(LV_PALETTE_GREY));
  lv_style_set_line_width(&minor_style, 2);
  lv_style_set_width(&minor_style, 5);
  lv_obj_add_style(scale_obj, &minor_style, LV_PART_ITEMS);

  // Sections 0-200 bleu / 200-400 vert / 400-500 rouge
  static lv_style_t cold_main, cold_ind, cold_items;
  lv_style_init(&cold_main);
  lv_style_set_arc_color(&cold_main, lv_palette_main(LV_PALETTE_BLUE));
  lv_style_set_arc_width(&cold_main, 5);

  lv_style_init(&cold_ind);
  lv_style_set_line_color(&cold_ind, lv_palette_darken(LV_PALETTE_BLUE, 2));
  lv_style_set_line_width(&cold_ind, 2);
  lv_style_set_width(&cold_ind, 10);

  lv_style_init(&cold_items);
  lv_style_set_line_color(&cold_items, lv_palette_lighten(LV_PALETTE_BLUE, 2));
  lv_style_set_line_width(&cold_items, 2);
  lv_style_set_width(&cold_items, 5);

  lv_scale_section_t * cold_sec = lv_scale_add_section(scale_obj);
  lv_scale_section_set_range(cold_sec, 0, 200);
  lv_scale_section_set_style(cold_sec, LV_PART_MAIN, &cold_main);
  lv_scale_section_set_style(cold_sec, LV_PART_INDICATOR, &cold_ind);
  lv_scale_section_set_style(cold_sec, LV_PART_ITEMS, &cold_items);

  static lv_style_t normal_main;
  lv_style_init(&normal_main);
  lv_style_set_arc_color(&normal_main, lv_palette_main(LV_PALETTE_GREEN));
  lv_style_set_arc_width(&normal_main, 5);

  lv_scale_section_t * normal_sec = lv_scale_add_section(scale_obj);
  lv_scale_section_set_range(normal_sec, 200, 400);
  lv_scale_section_set_style(normal_sec, LV_PART_MAIN, &normal_main);

  static lv_style_t hot_main, hot_ind, hot_items;
  lv_style_init(&hot_main);
  lv_style_set_arc_color(&hot_main, lv_palette_main(LV_PALETTE_RED));
  lv_style_set_arc_width(&hot_main, 5);

  lv_style_init(&hot_ind);
  lv_style_set_line_color(&hot_ind, lv_palette_darken(LV_PALETTE_RED, 2));
  lv_style_set_line_width(&hot_ind, 2);
  lv_style_set_width(&hot_ind, 10);

  lv_style_init(&hot_items);
  lv_style_set_line_color(&hot_items, lv_palette_lighten(LV_PALETTE_RED, 2));
  lv_style_set_line_width(&hot_items, 2);
  lv_style_set_width(&hot_items, 5);

  lv_scale_section_t * hot_sec = lv_scale_add_section(scale_obj);
  lv_scale_section_set_range(hot_sec, 400, 500);
  lv_scale_section_set_style(hot_sec, LV_PART_MAIN, &hot_main);
  lv_scale_section_set_style(hot_sec, LV_PART_INDICATOR, &hot_ind);
  lv_scale_section_set_style(hot_sec, LV_PART_ITEMS, &hot_items);

  // Aiguille (parent 240x240 transparent)
  static lv_obj_t* needle_parent = nullptr;
  needle_parent = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(needle_parent);
  lv_obj_set_size(needle_parent, 240, 240);
  lv_obj_align_to(needle_parent, scale_obj, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag(needle_parent, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  lv_obj_clear_flag(needle_parent, LV_OBJ_FLAG_SCROLLABLE);

  needle_img = lv_obj_create(needle_parent);
  lv_obj_remove_style_all(needle_img);

  const int needle_w = 4;
  const int needle_h = 90;

  lv_obj_set_size(needle_img, needle_w, needle_h);
  lv_obj_set_style_bg_color(needle_img, lv_color_hex(0xFF6600), 0);
  lv_obj_set_style_bg_opa(needle_img, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(needle_img, 2, 0);

  // base au centre
  lv_obj_set_pos(needle_img, 120 - needle_w/2, 120 - needle_h);
  lv_obj_set_style_transform_pivot_x(needle_img, needle_w/2, 0);
  lv_obj_set_style_transform_pivot_y(needle_img, needle_h, 0);

  // Label température (police 24)
  temp_label = lv_label_create(lv_screen_active());
  lv_obj_align(temp_label, LV_ALIGN_CENTER, 0, 80);
  lv_label_set_text(temp_label, "0°C");

  static lv_style_t temp_style;
  lv_style_init(&temp_style);
  lv_style_set_text_font(&temp_style, &lv_font_montserrat_24);
  lv_obj_add_style(temp_label, &temp_style, 0);

  // Bouton moteur
  motor_btn = lv_btn_create(lv_screen_active());
  lv_obj_set_size(motor_btn, 200, 50);
  lv_obj_align(motor_btn, LV_ALIGN_BOTTOM_MID, 0, -30);
  lv_obj_add_event_cb(motor_btn, motor_btn_event_cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_set_style_bg_color(motor_btn, lv_palette_main(LV_PALETTE_RED), 0);

  motor_btn_label = lv_label_create(motor_btn);
  lv_label_set_text(motor_btn_label, "Moteur");
  lv_obj_center(motor_btn_label);

  set_temperature(0);
}

/* ===================== LVGL setup ===================== */
static void lvgl_setup()
{
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

  // Init écran
  tft.init();
  tft.setBrightness(255);

  // Création display LVGL
  g_disp = lv_display_create(320, 480);
  lv_display_set_flush_cb(g_disp, lv_flush_cb);

  // Buffer PARTIAL en heap (évite overflow BSS)
  static constexpr uint32_t BUF_LINES = 20;
  const size_t buf_bytes = 320 * BUF_LINES * sizeof(lv_color_t);

  lv_color_t * buf1 = (lv_color_t*)malloc(buf_bytes);
  if(!buf1) {
    Serial.println("malloc draw buffer failed");
    while(1) delay(100);
  }

  lv_display_set_buffers(g_disp, buf1, nullptr, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Input device (touch)
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, lv_touch_read_cb);
  lv_indev_set_display(indev, g_disp);
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("Démarrage...");

  // Sécurise le GT911 avant toute init I2C interne
  gt911_reset_0x5D();

  // Sortie moteur
  pinMode(MOTOR_PIN, OUTPUT);

  // Init MAX6675 (bitbang)
  max6675_begin();

  lvgl_setup();
  create_ui();

  Serial.println("Interface créée!");
}

void loop()
{
  lv_timer_handler();

  static uint32_t last = 0;
  static uint32_t last2 = 0;
  uint32_t now = millis();
  if(now - last >= 1000) {              // MAX6675 conversion time
    last = now;

    uint16_t raw = max6675_read_raw();
    float tc = max6675_raw_to_c(raw);

    Serial.printf("MAX6675 raw=0x%04X open=%d temp=%0.2f\n",
                  raw, (raw & 0x0004) ? 1 : 0, isnan(tc) ? -999.0f : tc);

    if(!isnan(tc)) {
      current_temp = (int32_t)lroundf(tc);
      if(current_temp < 0) current_temp = 0;
      if(current_temp > 500) current_temp = 500;
      set_temperature(current_temp);
    } else {
      lv_label_set_text(temp_label, "NC");
    }
  }
  if(now - last2 >= 50) {             
    last2 = now;
    motor_set();
  }
  delay(5);

}

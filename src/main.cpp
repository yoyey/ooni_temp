#include <Arduino.h>
#include <lvgl.h>
#include "lgfx_setup.h"
#include <math.h>
#include "max6675.h"
extern lv_display_t * g_disp;
extern lv_obj_t * motor_btn;
extern lv_obj_t * temp_label;
extern bool motor_on;
static int32_t current_temp = 0;
static constexpr int MOTOR_PIN = 26;

/* ===================== GT911 reset (0x5D) ===================== */
static void gt911_reset_0x5D()
{
  constexpr int TP_RST = 25;
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(20);
  digitalWrite(TP_RST, HIGH);
  delay(120);
}

static void motor_set()
{
  digitalWrite(MOTOR_PIN, HIGH);
  delayMicroseconds(3);
  digitalWrite(MOTOR_PIN, LOW);
}

void setup()
{
  Serial.begin(115200);
  delay(300);
  Serial.println("Démarrage...");
  gt911_reset_0x5D();
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(MOTOR_PIN,LOW);
  digitalWrite(PIN_EN,HIGH);
  max6675_begin();
  lvgl_setup();
  create_ui();
}

void loop()
{
  lv_timer_handler();
  static uint32_t last = 0;
  static uint32_t last2 = 0;
  uint32_t now = millis();
  if(now - last >= 1000) {
    last = now;

    uint16_t raw = max6675_read_raw();
    float tc = max6675_raw_to_c(raw);

    if(!isnan(tc)) {
      current_temp = (int32_t)lroundf(tc);
      if(current_temp < 0) current_temp = 0;
      if(current_temp > 500) current_temp = 500;
      set_temperature(current_temp);
    } else {
      lv_label_set_text(temp_label, "NC");
    }
  }

  if(motor_on == true)
  {
    if(now - last2 >= 50) {             
      last2 = now;
      motor_set();
    }
  }
  delay(5);
}

#include <cstdint>
#include <math.h>
#include <Arduino.h>
#include "max6675.h"

constexpr int PIN_MAX_CS   = 22;  // 
constexpr int PIN_MAX_SCK  = 21;  // 
constexpr int PIN_MAX_SO   = 35;  //

void max6675_begin()
{
  pinMode(PIN_MAX_CS, OUTPUT);
  digitalWrite(PIN_MAX_CS, HIGH);

  pinMode(PIN_MAX_SCK, OUTPUT);
  digitalWrite(PIN_MAX_SCK, LOW);

  pinMode(PIN_MAX_SO, INPUT);
}

uint16_t max6675_read_raw()
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

float max6675_raw_to_c(uint16_t v)
{
  if (v & 0x0004) return NAN;  // thermocouple open
  v >>= 3;
  return v * 0.25f;
}
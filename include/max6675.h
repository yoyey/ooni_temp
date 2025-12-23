#pragma once
void max6675_begin();
uint16_t max6675_read_raw();
float max6675_raw_to_c(uint16_t v);
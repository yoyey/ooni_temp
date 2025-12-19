#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

/*
  ESP32-3248S035C (typique)
  LCD (ST7796 SPI):  SCK=14 MOSI=13 MISO=12 DC=2 CS=15 BL=27
  Touch capacitif (GT911 I2C): SDA=33 SCL=32 INT=21 RST=25, addr=0x5D
*/

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796  _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_GT911   _touch;

public:
  LGFX(void)
  {
    { // --- BUS SPI écran ---
      auto cfg = _bus.config();
      cfg.spi_host   = HSPI_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 55000000;
      cfg.freq_read  = 20000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = 1;

      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = 12;
      cfg.pin_dc   = 2;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    { // --- PANEL ---
      auto cfg = _panel.config();
      cfg.pin_cs   = 15;
      cfg.pin_rst  = -1;
      cfg.pin_busy = -1;

      cfg.memory_width  = 320;
      cfg.memory_height = 480;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable = true;

      cfg.invert    = false; // si écran noir/couleurs bizarres: tester true
      cfg.rgb_order = false; // si rouge/bleu inversés: tester true

      cfg.dlen_16bit = false;
      cfg.bus_shared = true;

      _panel.config(cfg);
    }

    { // --- BACKLIGHT ---
      auto cfg = _light.config();
      cfg.pin_bl = 27;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;

      _light.config(cfg);
      _panel.setLight(&_light);
    }

    { // --- TOUCH GT911 (I2C) ---
      auto cfg = _touch.config();
      cfg.i2c_port = 0;
      cfg.pin_sda  = 33;
      cfg.pin_scl  = 32;
      cfg.pin_int  = -1;
      cfg.pin_rst  = 25;
      cfg.freq     = 400000;  // si instable: 100000
      cfg.i2c_addr = 0x5D;    // trouvé via scan

      cfg.x_min = 0;
      cfg.x_max = 319;
      cfg.y_min = 0;
      cfg.y_max = 479;

      cfg.offset_rotation = 0;

      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }

    setPanel(&_panel);
  }
};

extern LGFX tft;

// LovyanGFX setup for a 2.4" ST7789 240x320 SPI module (8 pins: GND VCC SCL SDA RESET DC CS BL, no MISO).
#pragma once
#include <LovyanGFX.hpp>

struct RobotEyesPins {
  int sclk = 12;  // module "SCL"
  int mosi = 11;  // module "SDA"
  int cs = 10;
  int dc = 9;
  int rst = 14;   // module "RESET"
  int bl = 13;    // backlight, active HIGH; -1 if BL is wired straight to 3V3
};

class RobotEyesDisplay : public lgfx::LGFX_Device {
 public:
  explicit RobotEyesDisplay(const RobotEyesPins& pins = RobotEyesPins(), uint32_t spi_hz = 40000000) {
    {
      auto cfg = bus_.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = spi_hz;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = pins.sclk;
      cfg.pin_mosi = pins.mosi;
      cfg.pin_miso = -1;
      cfg.pin_dc = pins.dc;
      bus_.config(cfg);
      panel_.setBus(&bus_);
    }
    {
      auto cfg = panel_.config();
      cfg.pin_cs = pins.cs;
      cfg.pin_rst = pins.rst;
      cfg.pin_busy = -1;
      cfg.panel_width = 240;
      cfg.panel_height = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable = false;
      cfg.invert = false;  // some batches need true: RobotEyes::setInvert() flips it at runtime
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;
      panel_.config(cfg);
    }
    if (pins.bl >= 0) {
      auto cfg = light_.config();
      cfg.pin_bl = pins.bl;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      light_.config(cfg);
      panel_.setLight(&light_);
    }
    setPanel(&panel_);
  }

 private:
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_SPI bus_;
  lgfx::Light_PWM light_;
};

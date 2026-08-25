#include <stdio.h>
#include <stdlib.h>
#include "i2c_bsp.h"
#include "user_config.h"
#include "freertos/FreeRTOS.h"

static i2c_master_bus_handle_t peri_bus_handle = NULL;
static i2c_master_bus_handle_t touch_bus_handle = NULL;
i2c_master_dev_handle_t disp_touch_dev_handle = NULL;
i2c_master_dev_handle_t rtc_dev_handle = NULL;

static uint32_t i2c_xfer_ticks = 0;
static uint32_t i2c_done_ticks = 0;

void i2c_master_init(void)
{
  i2c_xfer_ticks = pdMS_TO_TICKS(5000);
  i2c_done_ticks = pdMS_TO_TICKS(1000);

  i2c_master_bus_config_t i2c_bus_config =
  {
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .i2c_port = I2C_NUM_0,
    .scl_io_num = PIN_PERI_SCL,
    .sda_io_num = PIN_PERI_SDA,
    .glitch_ignore_cnt = 7,
    .flags = {
      .enable_internal_pullup = true,
    },
  };
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &peri_bus_handle));
  i2c_bus_config.scl_io_num = PIN_TOUCH_SCL;
  i2c_bus_config.sda_io_num = PIN_TOUCH_SDA;
  i2c_bus_config.i2c_port = I2C_NUM_1;
  ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &touch_bus_handle));

  i2c_device_config_t dev_cfg =
  {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .scl_speed_hz = 300000,
  };
  dev_cfg.device_address = RTC_I2C_ADDR;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(peri_bus_handle, &dev_cfg, &rtc_dev_handle));

  dev_cfg.device_address = TOUCH_I2C_ADDR;
  ESP_ERROR_CHECK(i2c_master_bus_add_device(touch_bus_handle, &dev_cfg, &disp_touch_dev_handle));
}

uint8_t i2c_write_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len)
{
  if (i2c_master_bus_wait_all_done(peri_bus_handle, i2c_done_ticks) != ESP_OK) return ESP_FAIL;
  if (reg == -1) {
    return i2c_master_transmit(dev_handle, buf, len, i2c_xfer_ticks);
  }
  uint8_t *pbuf = (uint8_t *)malloc(len + 1);
  if (!pbuf) return 0xFF; /* callers treat any nonzero value as failure */
  pbuf[0] = (uint8_t)reg;
  for (uint8_t i = 0; i < len; i++) {
    pbuf[i + 1] = buf[i];
  }
  uint8_t ret = i2c_master_transmit(dev_handle, pbuf, len + 1, i2c_xfer_ticks);
  free(pbuf);
  return ret;
}

uint8_t i2c_master_write_read_dev(i2c_master_dev_handle_t dev_handle, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen)
{
  if (i2c_master_bus_wait_all_done(peri_bus_handle, i2c_done_ticks) != ESP_OK) return ESP_FAIL;
  return i2c_master_transmit_receive(dev_handle, writeBuf, writeLen, readBuf, readLen, i2c_xfer_ticks);
}

uint8_t i2c_read_buff(i2c_master_dev_handle_t dev_handle, int reg, uint8_t *buf, uint8_t len)
{
  if (i2c_master_bus_wait_all_done(peri_bus_handle, i2c_done_ticks) != ESP_OK) return ESP_FAIL;
  if (reg == -1) {
    return i2c_master_receive(dev_handle, buf, len, i2c_xfer_ticks);
  }
  uint8_t addr = (uint8_t)reg;
  return i2c_master_transmit_receive(dev_handle, &addr, 1, buf, len, i2c_xfer_ticks);
}

uint8_t i2c_master_touch_write_read(i2c_master_dev_handle_t dev_handle, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen)
{
  if (i2c_master_bus_wait_all_done(touch_bus_handle, i2c_done_ticks) != ESP_OK) return ESP_FAIL;
  return i2c_master_transmit_receive(dev_handle, writeBuf, writeLen, readBuf, readLen, i2c_xfer_ticks);
}

/* Touch health probe: distinguishes a dead AXS15231B (probe fails, lines
 * still high) from a clamped bus (a line reads low). Called at most every 5s
 * while touch reads keep failing (see touch_read_cb). */
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
void i2c_touch_diag(void)
{
  esp_err_t pr = i2c_master_probe(touch_bus_handle, TOUCH_I2C_ADDR, pdMS_TO_TICKS(100));
  ESP_LOGI("i2c1", "probe=0x%02X SCL=%d SDA=%d t=%lld", (int)pr,
           gpio_get_level((gpio_num_t)PIN_TOUCH_SCL), gpio_get_level((gpio_num_t)PIN_TOUCH_SDA),
           esp_timer_get_time() / 1000000);
}

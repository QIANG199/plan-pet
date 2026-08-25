#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_port.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "user_config.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "axs15231b/esp_lcd_axs15231b.h"
#include "i2c_bsp.h"

static const char *TAG = "lvgl_port";
static SemaphoreHandle_t lvgl_mux = NULL;

static uint16_t *trans_buf_1 = NULL;
uint8_t *lvgl_dest = NULL;                /* rotation buffer */
static SemaphoreHandle_t flush_done_semaphore;

#define LCD_BIT_PER_PIXEL 16
#define BYTES_PER_PIXEL (LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565))
#define BUFF_SIZE (LCD_H_RES * LCD_V_RES * BYTES_PER_PIXEL)


static const axs15231b_lcd_init_cmd_t lcd_init_cmds[] =
{
  {0x11, (uint8_t []){0x00}, 0, 100},
  {0x29, (uint8_t []){0x00}, 0, 100},
};

static bool port_flush_ready_isr(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
  BaseType_t TaskWoken;
  xSemaphoreGiveFromISR(flush_done_semaphore, &TaskWoken);
  return false;
}

static void port_tick_inc(void *arg)
{
  lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void flush_dma_chunks(esp_lcd_panel_handle_t panel_handle, uint8_t *pixels)
{
  const int flush_coun = (LVGL_FRAME_BUFF_LEN / LVGL_DMA_BUFF_LEN);
  const int offgap = (LCD_V_RES / flush_coun);
  const int dmalen = (LVGL_DMA_BUFF_LEN / 2);
  int offsetx1 = 0;
  int offsety1 = 0;
  int offsetx2 = LCD_H_RES;
  int offsety2 = offgap;

  uint16_t *map = (uint16_t *)pixels;
  xSemaphoreGive(flush_done_semaphore);
  for (int i = 0; i < flush_coun; i++)
  {
    xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
    memcpy(trans_buf_1, map, LVGL_DMA_BUFF_LEN);
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2, offsety2, trans_buf_1);
    offsety1 += offgap;
    offsety2 += offgap;
    map += dmalen;
  }
  xSemaphoreTake(flush_done_semaphore, portMAX_DELAY);
}

static void port_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * color_p)
{
  esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
  lv_draw_sw_rgb565_swap(color_p, lv_area_get_width(area) * lv_area_get_height(area));
#if DISPLAY_ROTATED
  lv_display_rotation_t rotation = lv_display_get_rotation(disp);
  if (rotation != LV_DISPLAY_ROTATION_0)
  {
    lv_area_t rotated_area;
    lv_color_format_t cf = lv_display_get_color_format(disp);
    /* Calculate the position of the rotated area */
    rotated_area = *area;
    lv_display_rotate_area(disp, &rotated_area);
    /* Source stride (bytes per line) of the area before rotation */
    uint32_t src_stride = lv_draw_buf_width_to_stride(lv_area_get_width(area), cf);
    /* Stride of the destination (rotated) area */
    uint32_t dest_stride = lv_draw_buf_width_to_stride(lv_area_get_width(&rotated_area), cf);

    int32_t src_w = lv_area_get_width(area);
    int32_t src_h = lv_area_get_height(area);
    lv_draw_sw_rotate(color_p, lvgl_dest, src_w, src_h, src_stride, dest_stride, rotation, cf);
    flush_dma_chunks(panel_handle, lvgl_dest);
  }
  else
  {
    flush_dma_chunks(panel_handle, color_p);
  }
#else
  flush_dma_chunks(panel_handle, color_p);
#endif
  lv_disp_flush_ready(disp);
}

/* Raw register protocol of the touch controller at 0x3b (from the Waveshare
 * demo): write magic header b5 ab a5 5a + a read request, point data comes
 * back in the same transaction. buff[1] = contact count; x/y are 12-bit
 * big-endian pairs at buff[2..5]. */
static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t *indevData)
{
  static int64_t lastDiag;
  static bool once;
  uint8_t read_touchpad_cmd[11] = {0xb5, 0xab, 0xa5, 0x5a, 0x0, 0x0, 0x0, 0x0e, 0x0, 0x0, 0x0};
  uint8_t buff[32] = {0};
  esp_err_t er = i2c_master_touch_write_read(disp_touch_dev_handle, read_touchpad_cmd, 11, buff, 32);
  int64_t nowUs = esp_timer_get_time();
  if (!once || (er != ESP_OK && nowUs - lastDiag > 5000000)) {
    once = true;
    lastDiag = nowUs;
    i2c_touch_diag();
  }
  uint16_t pointX;
  uint16_t pointY;
  pointX = (((uint16_t)buff[2] & 0x0f) << 8) | (uint16_t)buff[3];
  pointY = (((uint16_t)buff[4] & 0x0f) << 8) | (uint16_t)buff[5];
  if (buff[1] > 0 && buff[1] < 5)
  {
    indevData->state = LV_INDEV_STATE_PRESSED;
    if (pointX > LCD_V_RES) pointX = LCD_V_RES;
    if (pointY > LCD_H_RES) pointY = LCD_H_RES;
    indevData->point.x = pointY;
    indevData->point.y = (LCD_V_RES - pointX);
  }
  else
  {
    indevData->state = LV_INDEV_STATE_RELEASED;
  }
}

bool lvgl_port_lock(int timeout_ms)
{
  const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

void lvgl_port_unlock(void)
{
  assert(lvgl_mux && "lvgl_port_init must be called first");
  xSemaphoreGive(lvgl_mux);
}

static void lvgl_port_task(void *arg)
{
  uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
  for (;;)
  {
    if (lvgl_port_lock(-1))
    {
      task_delay_ms = lv_timer_handler();
      lvgl_port_unlock();
    }
    if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS)
    {
      task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    }
    else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS)
    {
      task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
    }
    vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
  }
}

void lvgl_port_init(void)
{
  flush_done_semaphore = xSemaphoreCreateBinary();
  assert(flush_done_semaphore);
  ESP_LOGI(TAG, "Initialize LCD RESET GPIO");

  gpio_config_t gpio_conf = {};
    gpio_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_conf.mode = GPIO_MODE_OUTPUT;
    gpio_conf.pin_bit_mask = ((uint64_t)0X01 << PIN_LCD_RST);
    gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
  ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_config(&gpio_conf));

  ESP_LOGI(TAG, "Initialize QSPI bus");
  spi_bus_config_t buscfg = {};
    buscfg.data0_io_num = PIN_LCD_DATA0;
    buscfg.data1_io_num = PIN_LCD_DATA1;
    buscfg.sclk_io_num = PIN_LCD_PCLK;
    buscfg.data2_io_num = PIN_LCD_DATA2;
    buscfg.data3_io_num = PIN_LCD_DATA3;
    buscfg.max_transfer_sz = LVGL_DMA_BUFF_LEN;
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  ESP_LOGI(TAG, "Install panel IO");
    esp_lcd_panel_io_handle_t panel_io = NULL;
    esp_lcd_panel_handle_t panel = NULL;

  esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = PIN_LCD_CS;
    io_config.dc_gpio_num = -1;
    io_config.spi_mode = 3;
    io_config.pclk_hz = 40 * 1000 * 1000;
    io_config.trans_queue_depth = 10;
    io_config.on_color_trans_done = port_flush_ready_isr;
    io_config.lcd_cmd_bits = 32;
    io_config.lcd_param_bits = 8;
    io_config.flags.quad_mode = true;
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &panel_io));

  axs15231b_vendor_config_t vendor_config = {};
    vendor_config.flags.use_qspi_interface = 1;
    vendor_config.init_cmds = lcd_init_cmds;
    vendor_config.init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);

  esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = -1;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = LCD_BIT_PER_PIXEL;
    panel_config.vendor_config = &vendor_config;

  ESP_LOGI(TAG, "Install panel driver");
  ESP_ERROR_CHECK(esp_lcd_new_panel_axs15231b(panel_io, &panel_config, &panel));

  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_RST, 1));
  vTaskDelay(pdMS_TO_TICKS(30));
  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_RST, 0));
  vTaskDelay(pdMS_TO_TICKS(250));
  ESP_ERROR_CHECK(gpio_set_level(PIN_LCD_RST, 1));
  vTaskDelay(pdMS_TO_TICKS(30));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

  lv_init();

  lv_display_t * disp = lv_display_create(LCD_H_RES, LCD_V_RES);
  lv_display_set_flush_cb(disp, port_flush_cb);

  uint8_t *buffer_1 = NULL;
  uint8_t *buffer_2 = NULL;
  buffer_1 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  buffer_2 = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM);
  assert(buffer_1);
  assert(buffer_2);
  trans_buf_1 = (uint16_t *)heap_caps_malloc(LVGL_DMA_BUFF_LEN, MALLOC_CAP_DMA);
  assert(trans_buf_1);
  lv_display_set_buffers(disp, buffer_1, buffer_2, BUFF_SIZE, LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_user_data(disp, panel);
#if DISPLAY_ROTATED
  lvgl_dest = (uint8_t *)heap_caps_malloc(BUFF_SIZE, MALLOC_CAP_SPIRAM); /* rotation buffer */
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
#endif
  /* Touch input device */
  lv_indev_t *touch_indev = lv_indev_create();
  lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch_indev, touch_read_cb);
  lv_indev_set_display(touch_indev, disp);

  ESP_LOGI(TAG, "Install LVGL tick timer");
  esp_timer_create_args_t lvgl_tick_timer_args = {};
    lvgl_tick_timer_args.callback = &port_tick_inc;
    lvgl_tick_timer_args.name = "lvgl_tick";
  esp_timer_handle_t lvgl_tick_timer = NULL;
  ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

  lvgl_mux = xSemaphoreCreateMutex();
  assert(lvgl_mux);
  xTaskCreatePinnedToCore(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL, 0);
}

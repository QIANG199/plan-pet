#pragma once

#include "lvgl.h"

/* Project-specific 16px CJK font (DengXian, 4bpp): ASCII 0x20-0x7E plus the
 * exact Chinese glyphs used by the settings page and a small reserve set.
 * The built-in simsun_16_cjk lacks some of these chars (e.g. 扫, 牌) which
 * rendered as tofu — hence this generated subset.
 *
 * Regenerate after changing any Chinese label (see the Opts header comment
 * in font_cn_16.c for the exact ranges):
 *   npx lv_font_conv --font C:\Windows\Fonts\Deng.ttf --size 16 --bpp 4 \
 *     --format lvgl --no-compress --no-prefilter --lv-include lvgl.h \
 *     --lv-font-name font_cn_16 -r 0x20-0x7E -r <codepoints> \
 *     -o firmware/src/ui/font_cn_16.c
 */
extern const lv_font_t font_cn_16;

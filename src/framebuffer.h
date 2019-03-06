#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "types.h"

typedef struct {
  uint16_t attributes;
  uint8_t win_a, win_b;
  uint16_t granularity;
  uint16_t winsize;
  uint16_t segment_a, segment_b;
  uint32_t unused_real_fct_ptr;
  uint16_t pitch; // bytes per scanline

  uint16_t x_res, y_res;
  uint8_t w_char, y_char, planes, bpp, banks;
  uint8_t memory_model, bank_size, image_pages;
  uint8_t reserved0;

  uint8_t red_mask, red_position;
  uint8_t green_mask, green_position;
  uint8_t blue_mask, blue_position;
  uint8_t rsv_mask, rsv_position;
  uint8_t directcolor_attributes;

  uint32_t physbase;
  uint32_t reserved1;
  uint16_t reserved2;
} __attribute__((packed))
vbe_mode_info_t;

void
framebuffer_init(const vbe_mode_info_t* mode_info, const uint8_t* font);

void
framebuffer_refresh();

#endif

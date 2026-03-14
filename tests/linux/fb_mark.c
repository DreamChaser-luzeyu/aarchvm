#include <fcntl.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

struct glyph {
  char ch;
  uint8_t rows[8];
};

static const struct glyph k_glyphs[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'B', {0x7c, 0x42, 0x42, 0x7c, 0x42, 0x42, 0x42, 0x7c}},
    {'F', {0x7e, 0x40, 0x40, 0x7c, 0x40, 0x40, 0x40, 0x40}},
    {'I', {0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e}},
    {'K', {0x42, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x41}},
    {'L', {0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x7e}},
    {'N', {0x42, 0x62, 0x52, 0x4a, 0x46, 0x42, 0x42, 0x42}},
    {'O', {0x3c, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c}},
    {'U', {0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3c}},
    {'X', {0x42, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x42}},
};

static const uint8_t *find_glyph(char ch) {
  size_t i;
  for (i = 0; i < sizeof(k_glyphs) / sizeof(k_glyphs[0]); ++i) {
    if (k_glyphs[i].ch == ch) {
      return k_glyphs[i].rows;
    }
  }
  return k_glyphs[0].rows;
}

static void put_pixel(uint8_t *base, uint32_t stride, uint32_t x, uint32_t y, uint32_t color) {
  uint32_t *pixel = (uint32_t *)(base + (size_t)y * stride + (size_t)x * 4u);
  *pixel = color;
}

static void fill_rect(uint8_t *base,
                      uint32_t stride,
                      uint32_t x,
                      uint32_t y,
                      uint32_t w,
                      uint32_t h,
                      uint32_t color) {
  uint32_t yy;
  uint32_t xx;
  for (yy = 0; yy < h; ++yy) {
    for (xx = 0; xx < w; ++xx) {
      put_pixel(base, stride, x + xx, y + yy, color);
    }
  }
}

static void draw_text(uint8_t *base,
                      uint32_t stride,
                      uint32_t x,
                      uint32_t y,
                      const char *text,
                      uint32_t fg,
                      uint32_t bg,
                      uint32_t scale) {
  size_t i;
  for (i = 0; text[i] != '\0'; ++i) {
    const uint8_t *glyph = find_glyph(text[i]);
    uint32_t gy;
    for (gy = 0; gy < 8; ++gy) {
      uint32_t gx;
      for (gx = 0; gx < 8; ++gx) {
        uint32_t color = (glyph[gy] & (1u << (7u - gx))) ? fg : bg;
        fill_rect(base,
                  stride,
                  x + (uint32_t)i * 9u * scale + gx * scale,
                  y + gy * scale,
                  scale,
                  scale,
                  color);
      }
    }
  }
}

int main(void) {
  int fd = open("/dev/fb0", O_RDWR);
  struct fb_fix_screeninfo fix;
  struct fb_var_screeninfo var;
  size_t map_len;
  uint8_t *fb;

  if (fd < 0) {
    return 0;
  }
  if (ioctl(fd, FBIOGET_FSCREENINFO, &fix) != 0 || ioctl(fd, FBIOGET_VSCREENINFO, &var) != 0) {
    close(fd);
    return 1;
  }
  if (var.bits_per_pixel != 32 || fix.line_length == 0) {
    close(fd);
    return 1;
  }

  map_len = fix.smem_len;
  fb = mmap(NULL, map_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (fb == MAP_FAILED) {
    close(fd);
    return 1;
  }

  fill_rect(fb, fix.line_length, 0, 0, var.xres, var.yres, 0x00102030u);
  fill_rect(fb, fix.line_length, 0, 0, var.xres, 28, 0x0020a040u);
  fill_rect(fb, fix.line_length, 0, 28, var.xres, 4, 0x00ffffffu);
  draw_text(fb, fix.line_length, 24, 48, "LINUX FB OK", 0x00ffffffu, 0x00102030u, 3u);

  msync(fb, map_len, MS_SYNC);
  munmap(fb, map_len);
  close(fd);
  return 0;
}

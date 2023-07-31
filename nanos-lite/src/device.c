#include <common.h>

#if defined(MULTIPROGRAM) && !defined(TIME_SHARING)
# define MULTIPROGRAM_YIELD() yield()
#else
# define MULTIPROGRAM_YIELD()
#endif

#define NAME(key) \
  [AM_KEY_##key] = #key,

static const char *keyname[256] __attribute__((used)) = {
  [AM_KEY_NONE] = "NONE",
  AM_KEYS(NAME)
};

size_t serial_write(const void *buf, size_t offset, size_t len) {
  for (size_t i = 0; i < len; ++i) putch(*((char *)buf + i));
  return len;
}

size_t events_read(void *buf, size_t offset, size_t len) {
  AM_INPUT_KEYBRD_T ev = io_read(AM_INPUT_KEYBRD);
  if (ev.keycode == AM_KEY_NONE) return 0;
  const char *name = keyname[ev.keycode];
  int ret = ev.keydown ? snprintf(buf, len, "kd %s\n", name) : snprintf(buf, len, "ku %s\n", name);
  return ret; // ret -> include terminating null character
}

size_t dispinfo_read(void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T ev = io_read(AM_GPU_CONFIG);
  int width = ev.width;
  int height = ev.height;

  int ret = snprintf(buf, len, "WIDTH : %d\nHEIGHT : %d", width, height);
  return ret; // ret -> include terminating null character
}

size_t fb_write(const void *buf, size_t offset, size_t len) {
  AM_GPU_CONFIG_T ev = io_read(AM_GPU_CONFIG);
  int width = ev.width;

  int y = offset / width;
  int x = offset - y * width;

  io_write(AM_GPU_FBDRAW, x, y, (void *)buf, len, 1, true);

  return len;
}

void init_device() {
  Log("Initializing devices...");
  ioe_init();
}

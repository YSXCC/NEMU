#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int evtdev = -1;
static int fbdev = -1;
static int screen_w = 0, screen_h = 0;
int canvas_w , canvas_h;
int canvas_relative_screen_w, canvas_relative_screen_h;

static void init_dispinfo() {
  int buf_size = 1024; // TODO: may be insufficient
  char * buf = (char *)malloc(buf_size * sizeof(char));
  int fd = open("/proc/dispinfo", 0, 0);
  int ret = read(fd, buf, buf_size);
  assert(ret < buf_size); // to be cautious...
  assert(close(fd) == 0);

  int i = 0;
  int width = 0, height = 0;

  assert(strncmp(buf + i, "WIDTH", 5) == 0);
  i += 5;
  for (; i < buf_size; ++i) {
    if (buf[i] == ':') { i++; break; }
    assert(buf[i] == ' ');
  }
  for (; i < buf_size; ++i) {
    if (buf[i] >= '0' && buf[i] <= '9') break;
    assert(buf[i] == ' ');
  }
  for (; i < buf_size; ++i) {
    if (buf[i] >= '0' && buf[i] <= '9') {
      width = width * 10 + buf[i] - '0';
    } else {
      break;
    }
  }
  assert(buf[i++] == '\n');

  assert(strncmp(buf + i, "HEIGHT", 6) == 0);
  i += 6;
  for (; i < buf_size; ++i) {
    if (buf[i] == ':') { i++; break; }
    assert(buf[i] == ' ');
  }
  for (; i < buf_size; ++i) {
    if (buf[i] >= '0' && buf[i] <= '9') break;
    assert(buf[i] == ' ');
  }
  for (; i < buf_size; ++i) {
    if (buf[i] >= '0' && buf[i] <= '9') {
      height = height * 10 + buf[i] - '0';
    } else {
      break;
    }
  }

  free(buf);

  screen_w = width;
  screen_h = height;
}

uint32_t NDL_GetTicks() {
  struct timeval tv;
  assert(gettimeofday(&tv, NULL) == 0);
  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

int NDL_PollEvent(char *buf, int len) {
  int fd = open("/dev/events", 0, 0);
  int ret = read(fd, buf, len);
  assert(close(fd) == 0);
  return ret == 0 ? 0 : 1;
}

void NDL_OpenCanvas(int *w, int *h) {
  if (*w == 0 && *h == 0) {
    *w = screen_w;
    *h = screen_h;
  }
  canvas_w = *w;
  canvas_h = *h;

  canvas_relative_screen_w = (screen_w - canvas_w) / 2;
  canvas_relative_screen_h = (screen_h - canvas_h) / 2;

  printf("canvas width : %d\ncanvas height : %d\n", canvas_w, canvas_h);

  if (getenv("NWM_APP")) {
    int fbctl = 4;
    fbdev = 5;
    screen_w = *w; screen_h = *h;
    char buf[64];
    int len = sprintf(buf, "%d %d", screen_w, screen_h);
    // let NWM resize the window and create the frame buffer
    write(fbctl, buf, len);
    while (1) {
      // 3 = evtdev
      int nread = read(3, buf, sizeof(buf) - 1);
      if (nread <= 0) continue;
      buf[nread] = '\0';
      if (strcmp(buf, "mmap ok") == 0) break;
    }
    close(fbctl);
  }
}

void NDL_DrawRect(uint32_t *pixels, int x, int y, int w, int h) {
  int fd = open("/dev/fb", 0, 0);
  for (int i = 0; i < h && y + i < canvas_h; ++i) {
    lseek(fd, (y + canvas_relative_screen_h + i) * screen_w + (x + canvas_relative_screen_w), SEEK_SET);
    write(fd, pixels + i * w, w < canvas_w - x ? w : canvas_w - x);
  }
  assert(close(fd) == 0); 
}

void NDL_OpenAudio(int freq, int channels, int samples) {
}

void NDL_CloseAudio() {
}

int NDL_PlayAudio(void *buf, int len) {
  return 0;
}

int NDL_QueryAudio() {
  return 0;
}

int NDL_Init(uint32_t flags) {
  if (getenv("NWM_APP")) {
    evtdev = 3;
  }
  init_dispinfo();
  return 0;
}

void NDL_Quit() {
}

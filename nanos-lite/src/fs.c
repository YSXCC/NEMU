#include <fs.h>

typedef size_t (*ReadFn) (void *buf, size_t offset, size_t len);
typedef size_t (*WriteFn) (const void *buf, size_t offset, size_t len);

typedef struct {
  char *name;
  size_t size;
  size_t disk_offset;
  ReadFn read;
  WriteFn write;
} Finfo;

enum {FD_STDIN, FD_STDOUT, FD_STDERR, DEV_EVENTS, PROC_DISPINFO, FD_FB};

size_t invalid_read(void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

size_t invalid_write(const void *buf, size_t offset, size_t len) {
  panic("should not reach here");
  return 0;
}

/* This is the information about all files in disk. */
static Finfo file_table[] __attribute__((used)) = {
  [FD_STDIN]  = {"stdin", 0, 0, invalid_read, invalid_write},
  [FD_STDOUT] = {"stdout", 0, 0, invalid_read, serial_write},
  [FD_STDERR] = {"stderr", 0, 0, invalid_read, serial_write},
  [DEV_EVENTS] = {"/dev/events", 0, 0, events_read, invalid_write},
  [PROC_DISPINFO] = {"/proc/dispinfo", 0, 0, dispinfo_read, invalid_write},
  [FD_FB] = {"/dev/fb", 0, 0, invalid_read, fb_write},
#include "files.h"
};

typedef struct {
  size_t fd;
  size_t open_offset; // relative to disk_offset
} OFinfo;

static OFinfo open_file_table[LENGTH(file_table)];
static size_t open_file_table_index = 0;

static int get_open_file_index(int fd) { // return -1 on failure
  int target_index = -1;
  for (int i = 0; i < open_file_table_index; ++i) {
    if (open_file_table[i].fd == fd) {
      target_index = i;
      break;
    }
  }
  return target_index;
}

void init_fs() {
  // TODO: initialize the size of /dev/fb
  AM_GPU_CONFIG_T ev = io_read(AM_GPU_CONFIG);
  int width = ev.width;
  int height = ev.height;
  file_table[FD_FB].size = width * height * 4;
}

extern size_t ramdisk_read(void *buf, size_t offset, size_t len);

extern size_t ramdisk_write(const void *buf, size_t offset, size_t len);

char* get_file_name(int fd) {
  return file_table[fd].name;
}

int fs_open(const char *pathname, int flags, int mode) {
  for (int i = 0; i < LENGTH(file_table); ++i) {
    if (strcmp(file_table[i].name, pathname) == 0) {
      if (i < FD_FB) {
        #ifdef STRACE
        Log("ignore open %s", pathname);
        #endif
        return i;
      }
      open_file_table[open_file_table_index].fd = i;
      open_file_table[open_file_table_index].open_offset = 0;
      open_file_table_index++;
      return i;
    }
  }
  panic("file %s not found", pathname);
  while (1);
}

size_t fs_read(int fd, void *buf, size_t len) {
  ReadFn readFn = file_table[fd].read;
  if (readFn != NULL) {
    return readFn(buf, 0, len);
  }

  int target_index = get_open_file_index(fd);
  if (target_index == -1) {
    #ifdef STRACE
    Log("file %s not open before read", file_table[fd].name);
    #endif
    return -1;
  }
  size_t read_len = len;
  size_t open_offset = open_file_table[target_index].open_offset;
  size_t size = file_table[fd].size;
  size_t disk_offset = file_table[fd].disk_offset;

  if (open_offset > size) return 0;

  if (open_offset + len > size) read_len = size - open_offset;
  ramdisk_read(buf, disk_offset + open_offset, read_len);
  open_file_table[target_index].open_offset += read_len;
  return read_len;
}

size_t fs_write(int fd, const void *buf, size_t len) {
  WriteFn writeFn = file_table[fd].write;
  if (writeFn != NULL && fd < FD_FB) {
    return writeFn(buf, 0, len);
  }

  int target_index = get_open_file_index(fd);

  if (target_index == -1) {
    #ifdef STRACE
    Log("file %s not open before write", file_table[fd].name);
    #endif
    return -1;
  }

  size_t write_len = len;
  size_t open_offset = open_file_table[target_index].open_offset;
  size_t size = file_table[fd].size;
  size_t disk_offset = file_table[fd].disk_offset;

  if (open_offset > size) return 0;

  if (open_offset + len > size) write_len = size - open_offset;
  writeFn ?
  writeFn      (buf, disk_offset + open_offset, write_len):
  ramdisk_write(buf, disk_offset + open_offset, write_len);
  open_file_table[target_index].open_offset += write_len;
  return write_len;
}

size_t fs_lseek(int fd, size_t offset, int whence) {
  if (fd < FD_FB) {
    #ifdef STRACE
    Log("ignore lseek %s", file_table[fd].name);
    #endif
    return 0;
  }

  int target_index = get_open_file_index(fd);
  if (target_index == -1) {
    #ifdef STRACE
    Log("file %s not open before lseek", file_table[fd].name);
    #endif
    return -1;
  }

  size_t new_offset = -1;
  size_t size = file_table[fd].size;
  size_t open_offset = open_file_table[target_index].open_offset;
  switch (whence) {
    case SEEK_SET:
      if (offset > size) new_offset = size;
        new_offset = offset;
      break;
    case SEEK_CUR:
      if (offset + open_offset > size) new_offset = size;
        new_offset = offset + open_offset;
      break;
    case SEEK_END:
      if (offset + size > size) new_offset = size;
        new_offset = offset + size;
      break;
    default:
      Log("Unknown whence %d", whence);
      return -1;
  }

  assert(new_offset >= 0);
  open_file_table[target_index].open_offset = new_offset;
  return new_offset;
}

int fs_close(int fd) {
  if (fd <= FD_FB) {
    #ifdef STRACE
    Log("ignore close %s", file_table[fd].name);
    #endif
    return 0;
  }

  int target_index = get_open_file_index(fd);

  if (target_index >= 0) {
    for (int i = target_index; i < open_file_table_index - 1; ++i) {
      open_file_table[i] = open_file_table[i + 1];
    }
    open_file_table_index--;
    assert(open_file_table_index >= 0);
    return 0;
  }
  #ifdef STRACE
  Log("file %s not open before close", file_table[fd].name);
  #endif
  return -1;
}
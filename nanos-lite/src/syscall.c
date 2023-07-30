#include <common.h>
#include "syscall.h"
#include <fs.h>



int sys_yield(){
  yield();
  return 0;
}

int sys_exit(Context *c){
  halt(0);
  return 0;
}

int sys_brk(Context *c) {
  return 0;
}

int sys_write(Context *c){
  int fd = c->GPR2;
  void *buf = (void *)c->GPR3;
  size_t count = c->GPR4;
  if (fd == 1 || fd == 2) {
    for (size_t i = 0; i < count; ++i) {
      putch(*((char *)buf + i));
    }
    return count;
  } else {
    return fs_write(fd, buf, count);
  }
  return -1;
}

int sys_open(Context *c){
  const char *pathname = (const char *)c->GPR2;
  int flags = c->GPR3;
  int mode = c->GPR4;
  #ifdef STRACE
    Log("Open file path name is:%s",pathname);
  #endif
  return fs_open(pathname, flags, mode);
}
int sys_read(Context *c){
  int fd = c->GPR2; 
  void *buf = (void *)c->GPR3; 
  size_t len = (size_t)c->GPR4;
  return fs_read(fd, buf, len);
}
int sys_close(Context *c){
  int fd = c->GPR2;
  return fs_close(fd);
}
int sys_lseek(Context *c){
  int fd = c->GPR2;
  size_t offset = (size_t)c->GPR3;
  int whence = c->GPR4;
  return fs_lseek(fd, offset, whence);
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1;
  a[1] = c->GPR2;
  a[2] = c->GPR3;
  a[3] = c->GPR4;
  #ifdef STRACE
    Log("STRACE EVENT_SYSCALL:%d",a[0]);
  #endif
  switch (a[0]) {
    case SYS_exit:
      c->GPRx = sys_exit(c);
      break;
    case SYS_yield:
      c->GPRx = sys_yield();
      break;
    case SYS_write:
      c->GPRx = sys_write(c);
      break;
    case SYS_brk:
      c->GPRx = sys_brk(c);
      break;
    case SYS_open:
      c->GPRx = sys_open(c);
      break;
    case SYS_read:
      c->GPRx = sys_read(c);
      break;
    case SYS_close:
      c->GPRx = sys_close(c);
      break;
    case SYS_lseek:
      c->GPRx = sys_lseek(c);
      break;
    default: panic("Unhandled syscall ID = %d", a[0]);
  }

}

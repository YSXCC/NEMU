#include <common.h>
#include "syscall.h"



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
  }
  return -1;
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
    default: panic("Unhandled syscall ID = %d", a[0]);
  }

}

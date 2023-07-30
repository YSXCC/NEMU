#include <common.h>

extern void do_syscall(Context *c);

static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_YIELD: 
    #ifdef STRACE
      Log("EVENT_YIELD");
    #endif
      break;
    case EVENT_SYSCALL:
    #ifdef STRACE
      Log("EVENT_SYSCALL");
    #endif
      do_syscall(c);
      break;
    case EVENT_PAGEFAULT:
    #ifdef STRACE
      Log("EVENT_PAGEFAULT");
    #endif
      break;
    default: panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}

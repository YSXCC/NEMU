#include <common.h>

static Context* do_event(Event e, Context* c) {
  switch (e.event) {
    case EVENT_YIELD: 
      Log("EVENT_YIELD");
      break;
    case EVENT_SYSCALL:
      Log("EVENT_SYSCALL");
      break;
    case EVENT_PAGEFAULT:
      Log("EVENT_PAGEFAULT");
      break;
    default: panic("Unhandled event ID = %d", e.event);
  }

  return c;
}

void init_irq(void) {
  Log("Initializing interrupt/exception handler...");
  cte_init(do_event);
}

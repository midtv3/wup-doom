/* libwup platform glue for doomgeneric. */
#pragma once
#include <stdint.h>

/* Called from the fatal-error path the engine was patched to use. */
void wup_doom_fail(const char *msg);
/* Poll the pad and queue keyboard events for doomgeneric's I_GetEvent(). */
void wup_doom_poll_input(void);

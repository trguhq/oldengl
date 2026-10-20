/* MIT licensed. See LICENSE. The bindings are currently single-threaded. */
#include "xgl_session.h"
#include <limits.h>
#include <string.h>

static Xgl_sys_state state;
static unsigned int references;
static unsigned long serial;
static char message_buffer[256];

void oldengl_xgl_set_error(const char *message)
{
    if (!message)
        message = "Unknown XGL error";
    strncpy(message_buffer, message, sizeof(message_buffer) - 1);
    message_buffer[sizeof(message_buffer) - 1] = '\0';
}

static void notify_error(Xgl_sys_state system)
{
    Xgl_error_info information;
    memset(&information, 0, sizeof(information));
    xgl_object_get(system, XGL_SYS_ST_ERROR_INFO, &information);
    ++serial;
    oldengl_xgl_set_error(information.msg);
}

Xgl_sys_state oldengl_xgl_acquire(void)
{
    if (references == UINT_MAX) {
        oldengl_xgl_set_error("Too many XGL session references");
        return 0;
    }
    if (!references) {
        unsigned long before = serial;
        state = xgl_open(XGL_SYS_ST_ERROR_DETECTION, (Xgl_boolean)1,
            XGL_SYS_ST_ERROR_NOTIFICATION_FUNCTION, notify_error, 0);
        if (!state || serial != before) {
            if (state)
                xgl_close(state);
            state = 0;
            if (!message_buffer[0])
                oldengl_xgl_set_error("xgl_open failed");
            return 0;
        }
    }
    ++references;
    return state;
}

void oldengl_xgl_release(void)
{
    if (references && --references == 0) {
        xgl_close(state);
        state = 0;
    }
}

Xgl_sys_state oldengl_xgl_system(void) { return state; }
unsigned long oldengl_xgl_error_serial(void) { return serial; }
const char *oldengl_xgl_last_error(void) { return message_buffer; }
void oldengl_xgl_clear_error(void) { message_buffer[0] = '\0'; }

/* MIT licensed. Shared process-wide XGL System State (XGL permits one). */
#ifndef OLDENGL_XGL_SESSION_H
#define OLDENGL_XGL_SESSION_H
#include <xgl/xgl.h>

Xgl_sys_state oldengl_xgl_acquire(void);
void oldengl_xgl_release(void);
Xgl_sys_state oldengl_xgl_system(void);
unsigned long oldengl_xgl_error_serial(void);
const char *oldengl_xgl_last_error(void);
void oldengl_xgl_clear_error(void);
void oldengl_xgl_set_error(const char *message);

#endif

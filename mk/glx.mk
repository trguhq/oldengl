# Complete XMesa/GLX source import. Native variants enable XGL hooks.
XMESA_SOURCES = \
    vendor/xmesa/src/fakeglx.c \
    vendor/xmesa/src/glxapi.c \
    vendor/xmesa/src/xfonts.c \
    vendor/xmesa/src/xm_api.c \
    vendor/xmesa/src/xm_dd.c \
    vendor/xmesa/src/xm_line.c \
    vendor/xmesa/src/xm_span.c \
    vendor/xmesa/src/xm_tri.c

XMESA_HEADERS = \
    vendor/xmesa/include/GL/glx.h \
    vendor/xmesa/include/GL/glxext.h \
    vendor/xmesa/include/GL/glxtokens.h \
    vendor/xmesa/include/GL/xmesa.h \
    vendor/xmesa/include/GL/xmesa_x.h \
    vendor/xmesa/include/GL/xmesa_xf86.h \
    vendor/xmesa/src/glxapi.h \
    vendor/xmesa/src/glxheader.h \
    vendor/xmesa/src/xfonts.h \
    vendor/xmesa/src/xmesaP.h \
    include/GL/glx.h \
    include/GL/glxext.h \
    include/GL/glxtokens.h

vendor/xmesa/src/fakeglx.xgl.o: vendor/xmesa/src/fakeglx.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/fakeglx.c -o $@

vendor/xmesa/src/glxapi.xgl.o: vendor/xmesa/src/glxapi.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/glxapi.c -o $@

vendor/xmesa/src/xfonts.xgl.o: vendor/xmesa/src/xfonts.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/xfonts.c -o $@

vendor/xmesa/src/xm_api.xgl.o: vendor/xmesa/src/xm_api.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/xm_api.c -o $@

vendor/xmesa/src/xm_dd.xgl.o: vendor/xmesa/src/xm_dd.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/xm_dd.c -o $@

vendor/xmesa/src/xm_line.xgl.o: vendor/xmesa/src/xm_line.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/xm_line.c -o $@

vendor/xmesa/src/xm_span.xgl.o: vendor/xmesa/src/xm_span.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/xm_span.c -o $@

vendor/xmesa/src/xm_tri.xgl.o: vendor/xmesa/src/xm_tri.c $(XMESA_HEADERS) $(MESA_HEADERS) src/xgl_accel.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -DOLDENGL_XGL_ACCEL -c vendor/xmesa/src/xm_tri.c -o $@

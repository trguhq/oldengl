# Portable make rules. GNU make or Solaris /usr/ccs/bin/make may be used.
# See README.md for Sun WorkShop and Solaris GCC command lines.
CC = cc
AR = ar
CFLAGS = -O2 -fPIC
CPPFLAGS =
SHARED_FLAGS = -shared
LDFLAGS =
LDLIBS = -lm
PYTHON = python3
PREFIX = /usr/local
DESTDIR =
XGL_ROOT = /opt/SUNWits/Graphics-sw/xgl
OPENWIN_ROOT = /usr/openwin
XGL_CPPFLAGS = -I$(XGL_ROOT)/include -I$(OPENWIN_ROOT)/include
XGL_LIBS = -L$(XGL_ROOT)/lib -L$(OPENWIN_ROOT)/lib -lxgl -lX11
X11_CPPFLAGS = -I$(OPENWIN_ROOT)/include
X11_LIBS = -L$(OPENWIN_ROOT)/lib -lX11

INCLUDES = -Iinclude -Ivendor/mesa/include -Ivendor/mesa/src -Ivendor/mesa/src/main -Ivendor/mesa/src/glapi -Ivendor/mesa/src/math
XMESA_INCLUDES = -Ivendor/xmesa/src -Ivendor/xmesa/include -Isrc
include mk/sources.mk
# Establish the default before including native XMesa compilation rules.
all: xgl
include mk/glx.mk
MESA_OBJECTS = $(MESA_SOURCES:.c=.o)
XMESA_OBJECTS = $(XMESA_SOURCES:.c=.o)
XMESA_ACCEL_OBJECTS = $(XMESA_SOURCES:.c=.xgl.o)
XGL_OBJECTS = src/xgl_backend.o src/xgl_pixels.o src/xgl_session.o src/xgl_accel.o src/xgl_xmesa.o src/xgl_batch.o src/xgl_lighting.o src/xgl_texture.o src/xgl_tnl.o src/xgl_hybrid.o src/xgl_fragments.o
PUBLIC_HEADERS = include/GL/gl.h include/GL/glext.h include/GL/osmesa.h

software: lib/liboldengl.a lib/liboldengl.so

xgl: lib/libXGLMesa.a lib/libXGLMesa.so lib/libGL.a lib/libGL.so

glx-software: lib/libGLsoft.a lib/libGLsoft.so

example: xgl examples/triangle examples/glx_triangle examples/glx_textured_cube

examples/glx_textured_cube: examples/glx_textured_cube.c lib/libGL.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(XGL_CPPFLAGS) $(LDFLAGS) -o $@ examples/glx_textured_cube.c lib/libGL.a $(XGL_LIBS) $(LDLIBS)

examples/glx_triangle: examples/glx_triangle.c lib/libGL.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(XGL_CPPFLAGS) $(LDFLAGS) -o $@ examples/glx_triangle.c lib/libGL.a $(XGL_LIBS) $(LDLIBS)

examples/triangle: examples/triangle.c lib/libXGLMesa.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(XGL_CPPFLAGS) $(LDFLAGS) -o $@ examples/triangle.c lib/libXGLMesa.a $(XGL_LIBS) $(LDLIBS)

$(MESA_OBJECTS): $(MESA_HEADERS) $(PUBLIC_HEADERS)

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(X11_CPPFLAGS) -c $< -o $@

lib/liboldengl.a: $(MESA_OBJECTS)
	mkdir -p lib
	$(AR) rcs $@ $(MESA_OBJECTS)

lib/liboldengl.so: $(MESA_OBJECTS)
	mkdir -p lib
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(MESA_OBJECTS) $(LDLIBS)

src/xgl_backend.o: src/xgl_backend.c include/GL/xglmesa.h src/xgl_pixels.h src/xgl_session.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_backend.c -o $@

src/xgl_session.o: src/xgl_session.c src/xgl_session.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_session.c -o $@

src/xgl_accel.o: src/xgl_accel.c src/xgl_accel.h src/xgl_accel_private.h src/xgl_lighting.h src/xgl_session.h $(XMESA_HEADERS) $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_accel.c -o $@

src/xgl_xmesa.o: src/xgl_xmesa.c src/xgl_accel.h src/xgl_tnl.h src/xgl_session.h $(XMESA_HEADERS) $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_xmesa.c -o $@

src/xgl_batch.o: src/xgl_batch.c src/xgl_accel_private.h src/xgl_tnl.h src/xgl_lighting.h src/xgl_texture.h src/xgl_pixels.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_batch.c -o $@

src/xgl_lighting.o: src/xgl_lighting.c src/xgl_lighting.h src/xgl_session.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_lighting.c -o $@

src/xgl_texture.o: src/xgl_texture.c src/xgl_texture.h src/xgl_session.h $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_texture.c -o $@

src/xgl_hybrid.o: src/xgl_hybrid.c src/xgl_accel_private.h src/xgl_accel.h src/xgl_fragments.h $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_hybrid.c -o $@

src/xgl_fragments.o: src/xgl_fragments.c src/xgl_fragments.h $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c src/xgl_fragments.c -o $@

src/xgl_tnl.o: src/xgl_tnl.c src/xgl_tnl.h $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c src/xgl_tnl.c -o $@

src/xgl_pixels.o: src/xgl_pixels.c src/xgl_pixels.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -c src/xgl_pixels.c -o $@

lib/libXGLMesa.a: $(MESA_OBJECTS) $(XMESA_ACCEL_OBJECTS) $(XGL_OBJECTS)
	mkdir -p lib
	$(AR) rcs $@ $(MESA_OBJECTS) $(XMESA_ACCEL_OBJECTS) $(XGL_OBJECTS)

lib/libXGLMesa.so: $(MESA_OBJECTS) $(XMESA_ACCEL_OBJECTS) $(XGL_OBJECTS)
	mkdir -p lib
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(MESA_OBJECTS) $(XMESA_ACCEL_OBJECTS) $(XGL_OBJECTS) $(XGL_LIBS) $(LDLIBS)

lib/libGL.a: lib/libXGLMesa.a
	cp lib/libXGLMesa.a $@

lib/libGL.so: lib/libXGLMesa.so
	cp lib/libXGLMesa.so $@

lib/libGLsoft.a: $(MESA_OBJECTS) $(XMESA_OBJECTS)
	mkdir -p lib
	$(AR) rcs $@ $(MESA_OBJECTS) $(XMESA_OBJECTS)

lib/libGLsoft.so: $(MESA_OBJECTS) $(XMESA_OBJECTS)
	mkdir -p lib
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(MESA_OBJECTS) $(XMESA_OBJECTS) $(X11_LIBS) $(LDLIBS)

tests/test_gl: tests/test_gl.c lib/liboldengl.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -o $@ tests/test_gl.c lib/liboldengl.a $(LDLIBS)

tests/xgl_backend_mock.o: src/xgl_backend.c include/GL/xglmesa.h src/xgl_pixels.h src/xgl_session.h $(MESA_HEADERS) tests/mock/X11/Xlib.h tests/mock/X11/Xutil.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -c src/xgl_backend.c -o $@

tests/xgl_session_mock.o: src/xgl_session.c src/xgl_session.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock -Isrc -c src/xgl_session.c -o $@

tests/test_xgl: tests/test_xgl.c tests/mock/mock_xgl.c tests/mock/mock_xgl.h tests/xgl_backend_mock.o tests/xgl_session_mock.o src/xgl_pixels.o lib/liboldengl.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock -Iinclude -Isrc -o $@ tests/test_xgl.c tests/mock/mock_xgl.c tests/xgl_backend_mock.o tests/xgl_session_mock.o src/xgl_pixels.o lib/liboldengl.a $(LDLIBS)

test: tests/test_gl tests/test_xgl
	./tests/test_gl
	./tests/test_xgl
	$(PYTHON) tests/check_api.py lib/liboldengl.a

tests/test_glx: tests/test_glx.c lib/libGLsoft.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(X11_CPPFLAGS) -o $@ tests/test_glx.c lib/libGLsoft.a $(X11_LIBS) $(LDLIBS)

tests/test_glx_accel: tests/test_glx_accel.c tests/mock_native_xgl.c tests/mock_native_xgl.h tests/mock/xgl/xgl.h lib/libGL.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -Itests/mock_sdk $(X11_CPPFLAGS) -o $@ tests/test_glx_accel.c tests/mock_native_xgl.c lib/libGL.a $(X11_LIBS) $(LDLIBS)

test-glx-accel: tests/test_glx_accel
	./tests/test_glx_accel

test-glx: tests/test_glx
	./tests/test_glx
	$(PYTHON) tests/check_glx_api.py lib/libGLsoft.a

tests/test_accel: tests/test_accel.c tests/mock/mock_xgl.c src/xgl_accel.c src/xgl_accel.h src/xgl_batch.c src/xgl_lighting.c src/xgl_texture.c tests/xgl_session_mock.o lib/liboldengl.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -Isrc tests/test_accel.c tests/mock/mock_xgl.c src/xgl_accel.c src/xgl_batch.c src/xgl_lighting.c src/xgl_texture.c src/xgl_pixels.c tests/xgl_session_mock.o lib/liboldengl.a $(LDLIBS) -o $@

tests/test_tnl: tests/test_tnl.c src/xgl_tnl.c src/xgl_tnl.h lib/liboldengl.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -Isrc -o $@ tests/test_tnl.c src/xgl_tnl.c lib/liboldengl.a $(LDLIBS)

tests/test_texture: tests/test_texture.c src/xgl_texture.c src/xgl_texture.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -Isrc -o $@ tests/test_texture.c src/xgl_texture.c $(LDLIBS)

test-texture: tests/test_texture
	./tests/test_texture

test-tnl: tests/test_tnl
	./tests/test_tnl

tests/test_fragments: tests/test_fragments.c src/xgl_fragments.c src/xgl_fragments.h lib/liboldengl.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -Isrc -o $@ tests/test_fragments.c src/xgl_fragments.c lib/liboldengl.a $(LDLIBS)

test-fragments: tests/test_fragments
	./tests/test_fragments

test-accel: tests/test_accel
	./tests/test_accel

install-software: software
	mkdir -p $(DESTDIR)$(PREFIX)/include/GL $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/oldengl
	cp $(PUBLIC_HEADERS) $(DESTDIR)$(PREFIX)/include/GL/
	cp lib/liboldengl.a lib/liboldengl.so $(DESTDIR)$(PREFIX)/lib/
	cp LICENSE README.md docs/XGL.md docs/GLX.md docs/ACCELERATION.md docs/VALIDATION.md $(DESTDIR)$(PREFIX)/share/doc/oldengl/
	cp vendor/mesa/LICENSE.MIT vendor/mesa/LICENSE.SGI-B-1.1 vendor/mesa/README.vendor $(DESTDIR)$(PREFIX)/share/doc/oldengl/

install: xgl install-software
	cp include/GL/xglmesa.h $(DESTDIR)$(PREFIX)/include/GL/
	cp include/GL/glx.h include/GL/glxext.h include/GL/glxtokens.h $(DESTDIR)$(PREFIX)/include/GL/
	cp lib/libXGLMesa.a lib/libXGLMesa.so lib/libGL.a lib/libGL.so $(DESTDIR)$(PREFIX)/lib/
	cp vendor/xmesa/README.vendor $(DESTDIR)$(PREFIX)/share/doc/oldengl/README.xmesa

install-xgl: install

clean:
	rm -f $(MESA_OBJECTS) $(XMESA_OBJECTS) $(XMESA_ACCEL_OBJECTS) $(XGL_OBJECTS) lib/*.a lib/*.so tests/test_gl tests/test_xgl tests/test_glx tests/test_accel tests/test_tnl tests/test_texture tests/test_fragments tests/test_glx_accel tests/*_mock.o examples/triangle examples/glx_triangle examples/glx_textured_cube

.PHONY: all software glx-software xgl example test test-glx test-accel test-tnl test-texture test-fragments test-glx-accel install install-xgl install-software clean

$(XMESA_OBJECTS): $(XMESA_HEADERS) $(MESA_HEADERS)

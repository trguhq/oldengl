# Portable make rules. GNU make or Solaris /usr/ccs/bin/make may be used.
# See README.md for Sun WorkShop and Solaris GCC command lines.
CC = cc
CXX = c++
AR = ar
CFLAGS = -O2 -fPIC
CXXFLAGS = -O2 -fPIC
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
include mk/glu.mk
MESA_OBJECTS = $(MESA_SOURCES:.c=.o)
XMESA_OBJECTS = $(XMESA_SOURCES:.c=.o)
XMESA_ACCEL_OBJECTS = $(XMESA_SOURCES:.c=.xgl.o)
XGL_OBJECTS = src/xgl_backend.o src/xgl_pixels.o src/xgl_session.o src/xgl_accel.o src/xgl_xmesa.o src/xgl_batch.o src/xgl_clip.o src/xgl_lighting.o src/xgl_texture.o src/xgl_tnl.o src/xgl_hybrid.o src/xgl_fragments.o
CORE_PUBLIC_HEADERS = include/GL/gl.h include/GL/glext.h include/GL/osmesa.h
PUBLIC_HEADERS = $(CORE_PUBLIC_HEADERS) include/GL/glu.h
# Reference libraries are private test fixtures, never installed products.
TEST_MESA_ARCHIVE = build/test/libmesa.a
TEST_MESA_SHARED = build/test/libmesa.so
TEST_GLX_ARCHIVE = build/test/libglx-reference.a

xgl: lib/libXGLMesa.a lib/libXGLMesa.so lib/libGL.a lib/libGL.so glu

example: xgl examples/triangle examples/glx_triangle examples/glx_textured_cube

examples/glx_textured_cube: examples/glx_textured_cube.c lib/libGL.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(XGL_CPPFLAGS) $(LDFLAGS) -o $@ examples/glx_textured_cube.c lib/libGL.a $(XGL_LIBS) $(LDLIBS)

examples/glx_triangle: examples/glx_triangle.c lib/libGL.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(XGL_CPPFLAGS) $(LDFLAGS) -o $@ examples/glx_triangle.c lib/libGL.a $(XGL_LIBS) $(LDLIBS)

examples/triangle: examples/triangle.c lib/libXGLMesa.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(XGL_CPPFLAGS) $(LDFLAGS) -o $@ examples/triangle.c lib/libXGLMesa.a $(XGL_LIBS) $(LDLIBS)

$(MESA_OBJECTS): $(MESA_HEADERS) $(CORE_PUBLIC_HEADERS)

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(X11_CPPFLAGS) -c $< -o $@

$(TEST_MESA_ARCHIVE): $(MESA_OBJECTS)
	mkdir -p build/test
	$(AR) rcs $@ $(MESA_OBJECTS)

$(TEST_MESA_SHARED): $(MESA_OBJECTS)
	mkdir -p build/test
	$(CC) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(MESA_OBJECTS) $(LDLIBS)

src/xgl_backend.o: src/xgl_backend.c include/GL/xglmesa.h src/xgl_pixels.h src/xgl_session.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_backend.c -o $@

src/xgl_session.o: src/xgl_session.c src/xgl_session.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_session.c -o $@

src/xgl_accel.o: src/xgl_accel.c src/xgl_accel.h src/xgl_accel_private.h src/xgl_lighting.h src/xgl_session.h $(XMESA_HEADERS) $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_accel.c -o $@

src/xgl_xmesa.o: src/xgl_xmesa.c src/xgl_accel.h src/xgl_tnl.h src/xgl_session.h $(XMESA_HEADERS) $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XMESA_INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_xmesa.c -o $@

src/xgl_batch.o: src/xgl_batch.c src/xgl_accel_private.h src/xgl_tnl.h src/xgl_clip.h src/xgl_lighting.h src/xgl_texture.h src/xgl_pixels.h $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_batch.c -o $@

src/xgl_clip.o: src/xgl_clip.c src/xgl_clip.h src/xgl_session.h $(MESA_HEADERS)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) $(XGL_CPPFLAGS) -c src/xgl_clip.c -o $@

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

$(TEST_GLX_ARCHIVE): $(MESA_OBJECTS) $(XMESA_OBJECTS)
	mkdir -p build/test
	$(AR) rcs $@ $(MESA_OBJECTS) $(XMESA_OBJECTS)

tests/test_gl: tests/test_gl.c $(TEST_MESA_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -o $@ tests/test_gl.c $(TEST_MESA_ARCHIVE) $(LDLIBS)

tests/xgl_backend_mock.o: src/xgl_backend.c include/GL/xglmesa.h src/xgl_pixels.h src/xgl_session.h $(MESA_HEADERS) tests/mock/X11/Xlib.h tests/mock/X11/Xutil.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -c src/xgl_backend.c -o $@

tests/xgl_session_mock.o: src/xgl_session.c src/xgl_session.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock -Isrc -c src/xgl_session.c -o $@

tests/test_xgl: tests/test_xgl.c tests/mock/mock_xgl.c tests/mock/mock_xgl.h tests/xgl_backend_mock.o tests/xgl_session_mock.o src/xgl_pixels.o $(TEST_MESA_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock -Iinclude -Isrc -o $@ tests/test_xgl.c tests/mock/mock_xgl.c tests/xgl_backend_mock.o tests/xgl_session_mock.o src/xgl_pixels.o $(TEST_MESA_ARCHIVE) $(LDLIBS)

test: tests/test_gl tests/test_xgl $(TEST_MESA_SHARED)
	./tests/test_gl
	./tests/test_xgl
	$(PYTHON) tests/check_api.py $(TEST_MESA_ARCHIVE)
	$(PYTHON) tests/check_api.py $(TEST_MESA_SHARED)

tests/test_glx: tests/test_glx.c $(TEST_GLX_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(X11_CPPFLAGS) -o $@ tests/test_glx.c $(TEST_GLX_ARCHIVE) $(X11_LIBS) $(LDLIBS)

tests/test_glx_accel: tests/test_glx_accel.c tests/mock_native_xgl.c tests/mock_native_xgl.h tests/mock/xgl/xgl.h lib/libGL.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -Itests/mock_sdk $(X11_CPPFLAGS) -o $@ tests/test_glx_accel.c tests/mock_native_xgl.c lib/libGL.a $(X11_LIBS) $(LDLIBS)

test-glx-accel: tests/test_glx_accel
	./tests/test_glx_accel

test-glx: tests/test_glx
	./tests/test_glx
	$(PYTHON) tests/check_glx_api.py $(TEST_GLX_ARCHIVE)

tests/test_accel: tests/test_accel.c tests/mock/mock_xgl.c src/xgl_accel.c src/xgl_accel.h src/xgl_batch.c src/xgl_clip.c src/xgl_clip.h src/xgl_lighting.c src/xgl_texture.c tests/xgl_session_mock.o $(TEST_MESA_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -Isrc tests/test_accel.c tests/mock/mock_xgl.c src/xgl_accel.c src/xgl_batch.c src/xgl_clip.c src/xgl_lighting.c src/xgl_texture.c src/xgl_pixels.c tests/xgl_session_mock.o $(TEST_MESA_ARCHIVE) $(LDLIBS) -o $@

tests/test_tnl: tests/test_tnl.c src/xgl_tnl.c src/xgl_tnl.h $(TEST_MESA_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -Isrc -o $@ tests/test_tnl.c src/xgl_tnl.c $(TEST_MESA_ARCHIVE) $(LDLIBS)

tests/test_texture: tests/test_texture.c src/xgl_texture.c src/xgl_texture.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -Isrc -o $@ tests/test_texture.c src/xgl_texture.c $(LDLIBS)

test-texture: tests/test_texture
	./tests/test_texture

test-tnl: tests/test_tnl
	./tests/test_tnl

tests/test_fragments: tests/test_fragments.c src/xgl_fragments.c src/xgl_fragments.h $(TEST_MESA_ARCHIVE)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(INCLUDES) -Isrc -o $@ tests/test_fragments.c src/xgl_fragments.c $(TEST_MESA_ARCHIVE) $(LDLIBS)

test-fragments: tests/test_fragments
	./tests/test_fragments

tests/test_clip: tests/test_clip.c src/xgl_clip.c src/xgl_clip.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -Isrc -o $@ tests/test_clip.c src/xgl_clip.c $(LDLIBS)

test-clip: tests/test_clip
	./tests/test_clip

tests/test_lighting: tests/test_lighting.c src/xgl_lighting.c src/xgl_lighting.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock $(INCLUDES) -Isrc -o $@ tests/test_lighting.c src/xgl_lighting.c $(LDLIBS)

test-lighting: tests/test_lighting
	./tests/test_lighting

tests/test_glu.o: tests/test_glu.c include/GL/glu.h include/GL/osmesa.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude -c tests/test_glu.c -o $@

tests/test_glu: tests/test_glu.o lib/libGLU.a $(TEST_MESA_ARCHIVE)
	$(CXX) $(LDFLAGS) -o $@ tests/test_glu.o lib/libGLU.a $(TEST_MESA_ARCHIVE) $(LDLIBS)

test-glu: tests/test_glu
	./tests/test_glu
	$(PYTHON) tests/check_glu_api.py lib/libGLU.a

tests/test_glu_glx.o: tests/test_glu_glx.c tests/mock_native_xgl.h include/GL/glu.h include/GL/glx.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Iinclude $(X11_CPPFLAGS) -c tests/test_glu_glx.c -o $@

tests/glu_mock_native_xgl.o: tests/mock_native_xgl.c tests/mock_native_xgl.h tests/mock/xgl/xgl.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -Itests/mock_sdk $(X11_CPPFLAGS) -c tests/mock_native_xgl.c -o $@

tests/test_glu_glx: tests/test_glu_glx.o tests/glu_mock_native_xgl.o lib/libGLU.a lib/libGL.a
	$(CXX) $(LDFLAGS) -o $@ tests/test_glu_glx.o tests/glu_mock_native_xgl.o lib/libGLU.a lib/libGL.a $(X11_LIBS) $(LDLIBS)

test-glu-glx: tests/test_glu_glx
	./tests/test_glu_glx

test-accel: tests/test_accel
	./tests/test_accel

install: xgl
	mkdir -p $(DESTDIR)$(PREFIX)/include/GL $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/oldengl
	cp $(PUBLIC_HEADERS) $(DESTDIR)$(PREFIX)/include/GL/
	cp LICENSE README.md docs/XGL.md docs/GLX.md docs/GLU.md docs/ACCELERATION.md docs/VALIDATION.md $(DESTDIR)$(PREFIX)/share/doc/oldengl/
	cp vendor/mesa/LICENSE.MIT vendor/mesa/LICENSE.SGI-B-1.1 vendor/mesa/README.vendor $(DESTDIR)$(PREFIX)/share/doc/oldengl/
	cp include/GL/xglmesa.h $(DESTDIR)$(PREFIX)/include/GL/
	cp include/GL/glx.h include/GL/glxext.h include/GL/glxtokens.h $(DESTDIR)$(PREFIX)/include/GL/
	cp lib/libXGLMesa.a lib/libXGLMesa.so lib/libGL.a lib/libGL.so lib/libGLU.a lib/libGLU.so $(DESTDIR)$(PREFIX)/lib/
	cp vendor/xmesa/README.vendor $(DESTDIR)$(PREFIX)/share/doc/oldengl/README.xmesa
	cp vendor/glu/README.vendor $(DESTDIR)$(PREFIX)/share/doc/oldengl/README.glu
	cp vendor/glu/LICENSE.SGI-B-1.1 $(DESTDIR)$(PREFIX)/share/doc/oldengl/LICENSE.GLU.SGI-B-1.1

install-xgl: install

clean:
	rm -f $(MESA_OBJECTS) $(XMESA_OBJECTS) $(XMESA_ACCEL_OBJECTS) $(XGL_OBJECTS) $(GLU_OBJECTS) $(TEST_MESA_ARCHIVE) $(TEST_MESA_SHARED) $(TEST_GLX_ARCHIVE) lib/*.a lib/*.so tests/test_gl tests/test_xgl tests/test_glx tests/test_accel tests/test_tnl tests/test_texture tests/test_fragments tests/test_clip tests/test_lighting tests/test_glu tests/test_glu.o tests/test_glu_glx tests/test_glu_glx.o tests/glu_mock_native_xgl.o tests/test_glx_accel tests/*_mock.o examples/triangle examples/glx_triangle examples/glx_textured_cube

.PHONY: all xgl glu example test test-glx test-accel test-tnl test-texture test-fragments test-clip test-lighting test-glu test-glu-glx test-glx-accel install install-xgl clean

$(XMESA_OBJECTS): $(XMESA_HEADERS) $(MESA_HEADERS)

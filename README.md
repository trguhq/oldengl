# OldenGL

OpenGL 1.1 with a client-side GLX 1.0–1.4 binding for **32-bit SPARC /
Solaris 2.6**. The library bundles the portable Mesa 4.0.4 renderer,
implements all 336 GL 1.1 entry points and all 39 core GLX entry points,
and dispatches eligible geometry and raster operations to Sun XGL. Mesa's
SGI GLU 1.3 implementation is included as `libGLU`.

The GLX binding provides visual/FBConfig selection, contexts and sharing,
windows, pixmaps, pixmap-backed pbuffers, double buffering, separate draw/read
surfaces, context copying, X fonts, and procedure lookup. It uses Xlib directly;
it does not implement the indirect GLX wire protocol or install an X server
extension. Explicit indirect-context requests return NULL.

Plain `make` and `make install` build and install the XGL-backed `libGL` and
the Mesa-based `libGLU` utility library.
Native dispatch is enabled by default, with software fallback when the current
operation or drawable cannot use the supported XGL path, or XGL fails.
The software renderer remains bundled for unsupported operations and
validation. GLU supplies utility calculations and geometry generation; its
rendering calls use the same OpenGL acceleration and fallback paths.

The GLX path submits eligible object-space geometry to XGL for model/view and
perspective transforms, clipping, ambient/diffuse lighting, texture mapping,
depth testing, and rasterization. This includes affine user clipping planes,
compatible positional spotlights, flat unlit homogeneous vertices, and
nonmipmapped 1D or eligible 2D textures.
It can draw to front and back window buffers.
When a draw needs software fragment effects, a hybrid path still rasterizes
each primitive with XGL, then applies Mesa alpha, stencil/depth, fog, blending,
logic, masks, and unsupported texture operations before committing pixels.
It preserves primitive order and shared color/depth results; unavailable capture
resources or unsupported rasterization use the full software renderer.
XGL selects its device implementation; actual hardware coverage depends on
the installed graphics adapter and driver.
See [acceleration coverage](docs/ACCELERATION.md) and [GLX details](docs/GLX.md)
for the supported combinations and remaining software operations.

The supplied manuals informed the API and presentation design. See
[the implementation notes](docs/XGL.md) for the XGL calls and manual page
references. The renderer's source, provenance, and third-party license
notices are included under [vendor/mesa](vendor/mesa/README.vendor). GLU has
separate [source and license records](vendor/glu/README.vendor).

## Build on Solaris 2.6

Install the Sun XGL SDK/runtime and OpenWindows development headers.
Build with 32-bit SPARC C and C++ compilers and the matching C++ runtime.
The rendering core uses C; the complete GLU implementation includes C++ NURBS
code. No Python, contemporary Mesa, OpenGL system library, or network download
is needed to build the libraries.
The defaults for the Sun packages are:

```
XGL_ROOT=/opt/SUNWits/Graphics-sw/xgl
OPENWIN_ROOT=/usr/openwin
```

With Sun WorkShop C/C++ and `make`:

```sh
make CC=cc CXX=CC 'CFLAGS=-O -Xa -KPIC' 'CXXFLAGS=-O -KPIC' 'SHARED_FLAGS=-G' \
  'XGL_LIBS=-L/opt/SUNWits/Graphics-sw/xgl/lib -R/opt/SUNWits/Graphics-sw/xgl/lib -L/usr/openwin/lib -R/usr/openwin/lib -lxgl -lX11 -ldga -ldl -lintl -lnsl -lsocket'
```

With Solaris GCC/G++, use `CC=gcc CXX=g++`, `CFLAGS='-O2 -ansi -fPIC'`,
`CXXFLAGS='-O2 -fPIC'` and `SHARED_FLAGS=-shared` instead. Set `XGL_ROOT`,
`OPENWIN_ROOT` and `XGL_LIBS` on the make command line when installed elsewhere. Use
`make clean` before changing compiler, architecture, or compile flags.
The portable sources are used, with no x86, VIS, or SPARC assembly dependency.

This produces `lib/libGL.a` and `lib/libGL.so`, also available under the
`libXGLMesa` name for existing callers, plus `lib/libGLU.a` and `lib/libGLU.so`.
Applications link against **one** GL implementation, plus XGL, X11 and libm.
Add `-lGLU` before `-lGL` when using GLU. Do not link another GL implementation
into the same process.

```sh
cc -Iinclude -I/usr/openwin/include application.c \
  -Llib -lGL \
  -L/opt/SUNWits/Graphics-sw/xgl/lib -lxgl \
  -L/usr/openwin/lib -lX11 -lm
```

For shared linking, supply runtime library paths with the compiler's
`-R` option or the site's normal runtime loader configuration. To stage
an installation, use `make install` (`install-xgl` is an alias) with the same
build variables and, for example, `PREFIX=/opt/oldengl DESTDIR=/tmp/stage`.

## Use GLU

Include `<GL/glu.h>` and link with `-lGLU -lGL`. GLU provides camera and
projection helpers, project/unproject, quadrics, polygon tessellation, NURBS,
image scaling and mipmap generation. Create and make a GLX context current
before using GLU operations that read or change GL state or render geometry.

For a C application using Sun WorkShop, compile as C and use the C++ driver
for the final link so static GLU linkage includes the matching C++ runtime:

```sh
cc -Iinclude -I/usr/openwin/include -c application.c -o application.o
CC application.o -Llib -lGLU -lGL \
  -L/opt/SUNWits/Graphics-sw/xgl/lib -lxgl \
  -L/usr/openwin/lib -lX11 -lm
```

`make glu` builds GLU separately; its shared library links to this project's
`libGL`, so that build still needs the native XGL SDK. See
[GLU details](docs/GLU.md) for utility behavior, source provenance and testing.

## Use GLX

Include `<GL/gl.h>` and `<GL/glx.h>`, choose a visual or FBConfig, create a
context with `direct=True`, then use ordinary `glXMakeCurrent`, GL calls,
and `glXSwapBuffers`. GLX double-buffered contexts have separate front and
back buffers. Eligible `GL_FRONT` and `GL_BACK` window rendering uses XGL;
back-buffer acceleration requires two allocated XGL window buffers. Hybrid
fragment processing also requires a hidden XGL buffer and readable native Z
for coverage, and supports alpha-buffer visuals. Color and depth transfers
preserve mixed native/software results. No opt-in variable is
needed: unset or zero `OLDENGL_NO_XGL` keeps XGL enabled;
`OLDENGL_NO_XGL=1` explicitly forces software rendering for comparison.

[examples/glx_textured_cube.c](examples/glx_textured_cube.c) demonstrates a
perspective-transformed, lit, textured, depth-tested rotating cube in a
double-buffered GLX window. Build it with `make example` and the same native
build variables, then run `examples/glx_textured_cube`. Press a key to quit,
or use `--frames 120` for a bounded run. The example needs no GLU or GLUT.

Run `make test-glx` with a valid `DISPLAY` to exercise the GLX binding against
its test-only Mesa/Xlib reference renderer. Set `X11_CPPFLAGS` and `X11_LIBS`
if X11 is outside the default OpenWindows location. This implementation
currently uses a single-threaded binding; no thread-safe GLX operation is
claimed.

## Use the original XGLMesa binding

Include `<GL/gl.h>` and `<GL/xglmesa.h>`. Create an InputOutput Xlib window
with a TrueColor or DirectColor visual and a matching colormap, then:

```c
XGLMesaContext context = XGLMesaCreateContext(display, window, NULL);
if (!context || !XGLMesaMakeCurrent(context)) {
    fprintf(stderr, "%s\n", XGLMesaGetLastError());
    /* Release application resources and return. */
}
/* Issue ordinary OpenGL calls here. */
glFlush();                         /* copy the software image through XGL */
/* After ConfigureNotify: */
XGLMesaResizeBuffers(context, width, height);
glViewport(0, 0, width, height);
/* After Expose, redraw or call XGLMesaSwapBuffers(context). */
/* Before destroying the window/display: */
XGLMesaDestroyContext(context);
```

The binding supplies a **single-buffer RGBA visual** with 8-bit color
components, 24-bit depth, 8-bit stencil and 16-bit accumulation components.
Use `GL_FRONT`; there is no `GL_BACK`, stereo or auxiliary color buffer.
`XGLMesaSwapBuffers` presents the single buffer; it does not exchange two
buffers. `glFlush` and `glFinish` present it as well. The maximum software
framebuffer is 2048 by 2048. Context creation does not make a context
current. `XGLMesaMakeCurrent(NULL)` releases it. The binding is
single-threaded, and it owns its XGL session; do not mix it with a separate
application-owned XGL session.

[examples/triangle.c](examples/triangle.c) contains complete Xlib window,
colormap, event, resize, and cleanup handling. Build it with `make example`
and the same Solaris build variables above, then run `examples/triangle`
on a machine with an XGL-capable X display. Press any key to exit.

The original `XGLMesa*` convenience binding remains a single-buffer software
renderer with XGL presentation. Use GLX for the new geometry/raster dispatch
and double-buffered contexts. The bundled GLU works with either binding;
GLUT remains a separate external library.
Color-index rendering remains available in software.

## Run development tests

The renderer and adapter tests can run headlessly on a development host
without the Sun XGL SDK:

```sh
make -j4 test
make test-accel
make test-tnl
make test-texture
make test-fragments
make test-clip
make test-lighting
make test-glu
```

These targets build internal reference libraries under `build/test/`, including
`libmesa.a` and `.so` for offscreen test contexts. `make test-glx` additionally
builds `build/test/libglx-reference.a` and requires an X server. These reference
libraries are test artifacts and are not installed. The behavioral test
executables do not require Python; the API symbol audits use Python 3 and
GNU/BSD-style `nm`. XGL adapter tests use test-only SDK mocks with the real
renderer; they do not require Sun headers or libraries.
The GLU tests link the static GLU library to the internal offscreen renderer
and need a C++ compiler, but no X server or Sun XGL SDK.

## Validation limits

Portable C builds and software behavior can be verified on Linux. The
native Solaris compiler, real XGL SDK, SPARC byte order and actual XGL
display hardware must also be checked on the target before deployment.
The tests are regression and integration tests, not the official OpenGL
conformance suite; no certification is claimed. The renderer is historical
code selected for the requested historical target, rather than a modern
Mesa release. See [docs/VALIDATION.md](docs/VALIDATION.md) for the checks
and a target-machine validation procedure.

New binding/build/test code uses the root MIT license. Retained Mesa and
SGI sources and headers keep their own notices and bundled licenses.

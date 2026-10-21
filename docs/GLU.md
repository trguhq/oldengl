# Mesa GLU

OldenGL includes the complete SGI GLU 1.3 implementation shipped with Mesa
4.0.4. It builds `lib/libGLU.a` and `lib/libGLU.so` and installs `<GL/glu.h>`.
The public API has C linkage; the NURBS implementation uses C++, so a matching
C++ compiler and runtime are required.

## Build and link

Plain `make`, `make xgl` and `make install` include GLU. `make glu` builds the
utility library; its shared-library dependency also builds the native
`libGL.so`. The static GLU archive can be built with `make lib/libGLU.a`
without the Sun XGL SDK.

Use the same architecture and compiler family for GLU, GL and the application.
The defaults are `CXX=c++` and `CXXFLAGS='-O2 -fPIC'`. For Sun WorkShop, use
`CXX=CC CXXFLAGS='-O -KPIC'` with the C and shared-library settings in the
[root README](../README.md). Solaris GCC/G++ builds use the corresponding
`gcc` and `g++` drivers. The target remains 32-bit SPARC Solaris 2.6.

Include `<GL/glu.h>` and place GLU before GL in the link order:

```sh
cc -Iinclude -c application.c -o application.o
CC application.o -Llib -lGLU -lGL \
  -L/opt/SUNWits/Graphics-sw/xgl/lib -lxgl \
  -L/usr/openwin/lib -lX11 -lm
```

Use `g++` for the final link when building with GCC/G++. Linking through the
C++ driver also supplies the runtime needed by static GLU's NURBS objects.
Configure runtime search paths for `libGLU.so`, `libGL.so`, XGL and the C++
runtime using the target compiler's `-R` settings or the site's loader setup.
`make install` installs both GLU libraries, its header and its license records.

## Utility behavior and acceleration

The library provides:

- Camera, projection and picking matrices, plus project/unproject helpers.
- Quadric spheres, cylinders, disks and partial disks, with normals and
  texture coordinates.
- Polygon tessellation, including concave contours, holes, winding rules and
  application callbacks for generated or combined vertices.
- NURBS curves and surfaces, trimming and rendering or tessellation callbacks.
- Image scaling and 1D, 2D and 3D mipmap construction.
- GLU version/extension queries and error descriptions.

GLU uses the application's current OpenGL context. It does not supply a window
binding; create a GLX context and make it current before calling utilities
that render or inspect GL state.

Matrix calculations, image processing, polygon tessellation and NURBS
subdivision execute in the utility library on the CPU. Quadric and NURBS
rendering issue ordinary GL calls. Tessellation callbacks can issue GL calls
or retain the generated mesh for later drawing. With a GLX context, those GL
calls use OldenGL's native XGL geometry, lighting, texture and raster paths
whenever eligible, including the same hybrid fragment processing and software
fallbacks as application-written geometry. Large meshes whose strips wrap
Mesa's immediate batches may retain software vertex setup and native
rasterization. GLU submission does not establish actual hardware execution;
that depends on the XGL driver and device.

## Source and validation

The implementation comes from `src/glu/sgi` and the matching public header in
the Mesa 4.0.4 tree, pinned to commit
`61b226a4f4d29a6f7654cac22ae96b76b139bd8f` in
[mesa-mirror](https://github.com/chaotic-cx/mesa-mirror). This is a separate
source snapshot from the NetBSD import used for OldenGL's renderer.
[vendor/glu/README.vendor](../vendor/glu/README.vendor), `SOURCE-MAP`,
`UPSTREAM-SHA256` and the retained file notices document the import and
applicable SGI license. The original `XGLMesa*` convenience binding continues
to render GLU-generated geometry in software and present it through XGL.

Run `make test-glu` on a development host to exercise GLU against the internal
offscreen Mesa renderer. These tests need C and C++ compilers but no X server
or Sun XGL SDK. They are behavior and integration checks, not official GLU
conformance certification. A real Sun compiler/runtime build and GLU rendering
through the target XGL display remain required before deployment.

The host suite passes 109 checks for matrices, scaling and mipmaps, rendered
quadrics, polygon tessellation and NURBS including trimming. All 59 public
functions are checked in both libraries. A full GLU ASan/UBSan build passes
the same behavioral tests with leak detection. Host fixture builds can also
run `make test-glu-glx` with Xvfb and the synthetic XGL SDK; 31 checks compare
sphere, disk and cylinder pixels with software rendering while requiring
native transforms, rasterization and depth tests. Both static and shared
linkage are validated. These fixtures do not replace target SDK/device tests.

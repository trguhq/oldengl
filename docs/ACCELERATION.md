# Native XGL geometry, lighting, textures and rasterization

The GLX build installs an object-space stage before Mesa's transform and
lighting stages. `src/xgl_tnl.c` imports immediate-mode and array attributes
and decomposes all ten GL 1.1 primitive modes into points, independent lines
and triangles, preserving winding and flat provoking colors. Eligible batches
carry their original object positions and normals into `src/xgl_batch.c`.
XGL performs model/view and projection transforms, six-plane clipping,
perspective division, eligible lighting and texturing, depth testing, and
rasterization through `xgl_multimarker`, `xgl_multipolyline` and
`xgl_triangle_list`.

These are XGL geometry operations. XGL chooses its device implementation;
calling them does not establish that every stage executes on graphics
hardware. Actual acceleration depends on the adapter and its XGL driver.
The native Sun SDK and target hardware have not been available for validation.

## Native state coverage

Native dispatch requires a front-left or back-left X window buffer, an RGB
TrueColor or DirectColor visual with at least eight bits per color channel,
no alpha buffer, and full RGB writes. Unsupported batches continue through
Mesa. `OLDENGL_NO_XGL=1` forces software rendering.

| Stage or operation | Native coverage and conditions |
| --- | --- |
| Transforms and clipping | Model/view and perspective projection, viewport and depth-range mapping, all six canonical clip planes; source vertex w must be one |
| Points | Unit size, unlit and untextured; no smoothing or attenuation |
| Lines | Unit width, unlit and untextured; no smoothing or stippling |
| Triangles | Filled front/back polygon modes; no smoothing, stippling or polygon offset |
| Face culling | GL projected winding is checked on the CPU; accepted vertices still enter XGL in object space |
| Lighting | Compatible Gouraud ambient/diffuse material lighting, emission and global ambient; directional or positional lights with constant/linear attenuation |
| Depth | All eight GL comparison functions and the depth write mask; 16- or 24-bit Mesa depth storage when the native Z range can represent it |
| Texture mapping | Eligible unit-zero 2D surface textures with per-pixel perspective correction; details below |
| Front color clear | Whole buffer, no scissor or partial RGB mask |
| Front color copy | Same draw/read drawable, identity zoom/transfer, in-bounds rectangle |
| Back color rendering | A second XGL window buffer must be allocated; retained XMesa color is uploaded and downloaded around native batches |

XGL's lighting equations differ from GL in some cases. Native lighting
therefore rejects specular contributions, spotlights, quadratic attenuation,
two-sided lighting, color material, local-viewer and separate-specular modes.
Spatially varying positional ambient attenuation also uses software.
The model/view matrix must be affine, and transformed normals must have unit
length. With `GL_NORMALIZE`, input normals are conditioned to satisfy XGL's
unit-normal contract. Nonunit normals without normalization use Mesa.

A legacy raster dispatch in `src/xgl_accel.c` can also send already transformed
Mesa vertices to XGL. Before this path runs, object transforms, clipping,
lighting, texture maps and native depth state are reset so they cannot be
applied twice. Thus some scenes with software vertex processing can still
use eligible XGL raster operations.

## Texture mapping

`src/xgl_texture.c` maps the active GL texture object to a native XGL MipMap
Texture and Texture Map. It uses the application's uploaded images, including
explicit mip levels, and compares fetched texels before each batch so subimage
updates and shared-context changes invalidate stale native data. GL image rows
and XGL raster rows are converted without relying on host byte order.

The XGL Programmer's Guide, page 471, states that per-pixel texture mapping
uses software when the hardware does not support it. Native XGL submission
therefore does not itself establish hardware texture acceleration.

The native subset supports:

- Unit-zero `GL_TEXTURE_2D`, with power-of-two, uncompressed, borderless images.
- RGB, RGBA, alpha, luminance, luminance-alpha and intensity base formats,
  subject to the GL texture environment's defined combinations.
- `GL_MODULATE`, `GL_REPLACE`, `GL_BLEND`, and RGB/RGBA `GL_DECAL` environments.
- `GL_REPEAT`; `GL_CLAMP_TO_EDGE` is also mapped when used through the supplied
  later-feature headers. GL 1.1 `GL_CLAMP` uses software because its border
  sampling semantics differ from XGL's clamp modes.
- Nearest and linear magnification, and all six GL 1.1 minification filters.
  Linear magnification combined with either nearest-texel mipmap filter uses
  software because GL and XGL choose different minification transition levels.
  Native mipmapping requires a square image chain; rectangular chains with
  an additional one-dimensional tail use software.
- Explicit texture coordinates, object/eye linear texgen, and the GL texture
  matrix. The transformed texture q must be the same nonzero value throughout
  a batch; varying projective q uses software.

Textures on points or lines, multiple texture units, 1D/3D/cube textures,
sphere-map texgen, compressed images, anisotropy, texture comparison and
unsupported LOD constraints use Mesa. Alpha-buffer visuals and alpha tests
or framebuffer blending also use software, even when the texture itself is
otherwise eligible.

## Shared color and depth results

The front X window is the common front color surface. Pending software pixel
batches are drained before native access, Xlib is synchronized before direct
access, and XGL is synchronized before subsequent software access. Software
blending and readback can therefore consume native results. OpenGL front
pixel ownership rules still apply to obscured window regions.

Mesa's retained back color image and depth image remain authoritative between
native batches. Before native depth testing, depth values are copied into the
native Z buffer; after successful rendering they are copied back for GL
readback and software fallback. Color-free depth probes measure whether each
native primitive rounds or truncates Z, and viewport mapping compensates for
Mesa's conversion. Unrecognized conversion or incompatible mixed-primitive
mappings use software. Device interpolation precision still needs target
validation. Back color follows the same upload/render/
download sequence through a second XGL window buffer. `glXSwapBuffers`
presents the updated XMesa image. This does not promise native page flipping
or vertical synchronization. Transfers and synchronization prioritize correct
mixed rendering; no performance improvement has been measured.

Stencil, occlusion, alpha tests, framebuffer blending, logic operations, fog,
scissor, user clip planes, partial RGB masks, alpha-buffer visuals, pixmaps,
pbuffers, selection and feedback use software. Native context creation or
device errors disable the affected native path and retain software fallback.
`src/xgl_session.c` shares the process-wide XGL System State.

The original `XGLMesa*` convenience binding is separate: it renders to a
single software RGBA image and presents that image through XGL. Use GLX for
this native geometry pipeline and double-buffered contexts.

## Manual and validation references

The implementation follows the supplied XGL 3.2 Programmer's Guide transform,
lighting, raster, buffering and texture-mapping chapters, plus the Reference
Manual entries for point/facet lists, context transformations and clip bounds,
light objects, native Z-buffer comparison/copy, window raster buffers,
`XGL_TMAP_DESCRIPTOR` and `XGL_MIPMAP_TEXTURE_MEM_RAS_LIST`. Texture details
follow the Programmer's Guide, chapter 17: pages 455–459 cover 32-bit
`0xAABBGGRR` texels and application mip levels; pages 463–465 cover final-color
composition; pages 468–472 cover filtering, perspective correction and data
coordinates; and pages 481–483 specify bottom-left texture coordinates.

Tests with synthetic SDK declarations and an Xlib-backed XGL fixture check
command routing, buffer exchanges and fallback behavior. They do not verify
Sun ABI, raster edge coverage, device precision or hardware performance.
See [VALIDATION.md](VALIDATION.md) for target-machine checks.

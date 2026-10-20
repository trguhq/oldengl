# Native XGL geometry, lighting, textures and rasterization

The default build and installation select the XGL-backed GLX library.
Rendering tries its object-space XGL path first, then the native raster path
for Mesa-processed vertices, then hybrid native rasterization with software
fragment processing before using full software rasterization when needed.
No environment-variable opt-in is required. `make software`,
`make glx-software` and `make install-software` are explicit software choices.

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

## Direct native state coverage

Native dispatch requires a front-left or back-left X window buffer, an RGB
TrueColor or DirectColor visual with at least eight bits per color channel,
no alpha buffer, and full RGB writes. Other draws continue through Mesa
vertex processing and can use the hybrid path described below.
`OLDENGL_NO_XGL=1` explicitly forces software rendering; leaving it unset or
setting it to zero keeps native dispatch enabled.

State that leaves fragments unchanged does not force software: alpha testing
with `GL_ALWAYS`, color logic with `GL_COPY`, additive blending with source
`GL_ONE` and destination `GL_ZERO`, and a scissor covering the full drawable
remain eligible. Color logic takes precedence over blending as GL requires.
Pixel-copy eligibility ignores polygon, lighting and texture state that does
not apply to `glCopyPixels`.

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
| Front color clear | Whole buffer, scissor disabled or covering the drawable, full RGB mask |
| Front color copy | Same draw/read drawable, identity zoom/transfer, in-bounds rectangle |
| Back color rendering | A second XGL window buffer must be allocated; retained XMesa color is uploaded and downloaded around native batches |

XGL's lighting equations differ from GL in some cases. Native lighting
therefore rejects specular contributions, spotlights, quadratic attenuation,
two-sided lighting, color material and separate-specular modes. Local-viewer
state does not block the supported zero-specular lighting cases.
Spatially varying positional ambient attenuation also uses software.
The model/view matrix must be affine, and transformed normals must have unit
length. With `GL_NORMALIZE`, input normals are conditioned to satisfy XGL's
unit-normal contract. Nonunit normals without normalization use Mesa.

A legacy raster dispatch in `src/xgl_accel.c` can also send already transformed
Mesa vertices to XGL. Before this path runs, object transforms, clipping,
lighting, texture maps and native depth state are reset so they cannot be
applied twice. Thus some scenes with software vertex processing can still
use eligible XGL raster operations.

## Hybrid fragment processing

`src/xgl_hybrid.c` captures one Mesa-processed point, line or filled triangle
in a nondisplayed XGL window buffer. XGL supplies primary RGB and coverage;
temporary native Z distinguishes covered fragments from untouched pixels,
including fragments whose color is black. Both readbacks must succeed before
any application color, alpha, depth or stencil is changed. On capture failure,
the untouched primitive can be rendered by the ordinary software rasterizer.

`src/xgl_fragments.c` reconstructs alpha, GL depth, fog and texture attributes
from the post-TNL vertices and feeds contiguous covered spans into Mesa's
existing fragment pipeline. Triangle depth reconstruction reuses Mesa's snapped
vertex positions and stepped Z interpolation, preserving varying-Z
`GL_EQUAL` comparisons with software draws. This reconstructs attributes only;
fragment coverage always comes from XGL. Enabled alpha tests, stencil/depth, fog, blending,
logic operations, channel masks, scissor, polygon stipple, separate specular
color and occlusion results are applied there. Hybrid depth and stencil tests
run in Mesa so software alpha/stencil dependencies retain GL ordering. The
direct native path remains preferred when all fragment state is compatible.
CPU vertex processing also handles lighting and clipping modes outside the
direct native subset. This path supports alpha-buffer visuals, front/back,
`GL_FRONT_AND_BACK` and `GL_NONE` destinations.

Every primitive is captured and committed before the next primitive starts.
Capturing a whole draw as one image would lose overlapping transparent
fragments and alpha/stencil side effects. The implementation preserves those
individual fragments rather than applying effects to a completed image.

Compatible unit-zero 2D RGB texture sampling can remain in XGL. Reconstructed
clip W preserves perspective correction after software vertex processing.
Because XGL does not export framebuffer alpha, Mesa samples the texture for
alpha, discards that sampling's RGB result, and retains the native RGB result.
Other texture modes use Mesa sampling on XGL-covered fragments. The texture
stage is applied once to the retained RGB; subsequent fragment effects use
the same Mesa span pipeline.

Hybrid capture requires RGB TrueColor or DirectColor windows with at least
eight bits per color channel, an allocated hidden XGL buffer, and readable
native Z. It supports unit, nonsmoothed points and lines, and nonsmoothed
filled triangles. Polygon stipple, two-sided materials and polygon offset can
be handled around native rasterization. Large or attenuated points, wide or
stippled lines, antialiasing, color-index rendering, pixmaps, pbuffers,
selection/feedback, invalid projective interpolants and unavailable capture
resources retain full software rendering.

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
compressed images, anisotropy, texture comparison and
unsupported LOD constraints use Mesa texture sampling. Where capture is
available, this preserves XGL rasterization. Alpha-buffer visuals and effective
alpha tests or blending can also use the hybrid path. Software sphere-map
texgen can feed native RGB texture sampling after vertex processing.

## Shared color and depth results

The front X window is the common front color surface. Pending software pixel
batches are drained before native access, Xlib is synchronized before direct
access, and XGL is synchronized before subsequent software access. Software
blending and readback can therefore consume native results. OpenGL front
pixel ownership rules still apply to obscured window regions.

Mesa's retained back color image and depth image remain authoritative between
native batches. Before native depth testing, depth values are copied into the
native Z buffer; after successful rendering they are copied back for GL
readback and software fallback. Depth probes in the hidden buffer measure
whether each native primitive rounds or truncates Z, and viewport mapping
compensates for Mesa's conversion. Unrecognized conversion or incompatible
mixed-primitive mappings use software. Device interpolation precision still
needs target validation. Back color follows the same upload/render/
download sequence through a second XGL window buffer. `glXSwapBuffers`
presents the updated XMesa image. This does not promise native page flipping
or vertical synchronization. Transfers and synchronization prioritize correct
mixed rendering; no performance improvement has been measured. Hybrid capture
adds synchronization and readbacks for every primitive, so it can be slower
than software. XGL window copies in obscured regions are device-dependent;
confirm pixel ownership and readback behavior on the target driver.

Hybrid fragment processing uses the same authoritative Mesa color, alpha,
depth and stencil storage. Scratch native Z never becomes application GL depth.
Native context creation or device errors disable the affected native path and
retain software fallback. Pixmaps, pbuffers and selection/feedback continue
through their existing software implementations.
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
The Reference Manual, page 234, limits `XGL_RENDER_Z_BUFFER` to raster
operations: geometry probes and capture use `XGL_RENDER_DRAW_BUFFER`, with
Z writes controlled by HLHSR. The hidden buffer is never selected for display.

Tests with synthetic SDK declarations and an Xlib-backed XGL fixture check
command routing, buffer exchanges and fallback behavior. They do not verify
Sun ABI, raster edge coverage, device precision or hardware performance.
See [VALIDATION.md](VALIDATION.md) for target-machine checks.

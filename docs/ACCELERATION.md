# Native XGL geometry and raster dispatch

`src/xgl_accel.c` submits supported primitives to `xgl_multimarker`,
`xgl_multipolyline`, and `xgl_triangle_list` on an XGL 3D window context.
It also uses `xgl_context_new_frame` for whole color clears and
`xgl_context_copy_buffer` for eligible identity color copies.
`src/xgl_xmesa.c` installs this dispatch in XMesa's software-rasterizer
selection callbacks. These are actual XGL operations, not software replay
followed by an image upload. XGL chooses the device implementation; calling
XGL does not establish that a particular driver executes on graphics hardware.

Mesa performs vertex transformation, lighting, primitive decomposition,
clipping, and viewport mapping. The resulting vertices carry GL colors into
XGL. Coordinates are converted from GL's lower-left origin and half-integer
sample centers to XGL device coordinates. Flat provoking colors and face
culling are handled before submission.

Native dispatch requires a front-left X window buffer, an RGB TrueColor or
DirectColor visual with at least 8-bit channels, and no alpha buffer. The
initial supported state combinations are intentionally limited:

| Operation | Additional requirements |
| --- | --- |
| Points | Unit size; no smoothing or attenuation |
| Lines | Unit width; no smoothing or stippling |
| Triangles | Filled front/back polygon modes; no smoothing or stippling |
| Color clear | Entire buffer; no scissor or partial RGB mask |
| Color copy | Same front draw/read drawable, identity zoom/transfer, in-bounds rectangle |

Depth/stencil tests, occlusion tests, alpha tests, blending, color logic
operations, textures, fog, separate-specular color, scissor and partial RGB
masks make geometry use software. Software also handles back buffers,
pixmaps, pbuffers, alpha-buffer visuals, selection and feedback. These
features remain functional; they are not approximated by an incompatible
native operation. `OLDENGL_NO_XGL=1` forces the software path.

The front window is the common color surface. Pending software pixel batches
are drained before native access, Xlib is synchronized before direct access,
and XGL is synchronized before subsequent software access. Thus software
blending/readback can consume native results, and native operations can follow
software writes. This avoids downloading an obscured window into an allegedly
authoritative retained buffer. OpenGL front-buffer pixel ownership rules still
apply. Software depth and stencil remain authoritative because operations
using them do not take the native path.

`src/xgl_session.c` shares the one process-wide XGL System State among
contexts and the original XGLMesa binding. Native context creation or device
errors disable the affected native path and preserve software fallback.
Synchronization at primitive boundaries prioritizes correctness; no performance
improvement or native hardware coverage has been measured here.

The implementation follows the supplied XGL Reference Manual entries for
point/facet lists, marker and line selectors, interpolated surface colors,
VDC device mapping, window rasters, context flush, new frame, and buffer copy.
Actual SDK compilation, raster edge coverage, and target device execution
still require Solaris/SPARC validation. Tests using synthetic SDK types verify
dispatch and fallback logic, not Sun ABI or hardware performance.

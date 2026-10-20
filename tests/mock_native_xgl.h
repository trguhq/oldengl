/* MIT licensed. Test-only XGL implementation backed by real Xlib windows. */
#ifndef OLDENGL_MOCK_NATIVE_XGL_H
#define OLDENGL_MOCK_NATIVE_XGL_H

struct MockNativeXGLStats {
    unsigned long points, lines, triangles, clears, copies;
    unsigned long transforms, lights, depth_tests, swaps;
    unsigned long spotlit_vertices;
    unsigned long texture_maps, mipmaps, texture_samples;
    unsigned long model_clip_primitives, model_clip_intersections;
    unsigned long homogeneous_vertices;
    unsigned long uploads, downloads, synchronizes;
    unsigned long opens, closes, live_objects, injected_errors;
};

void MockNativeXGLReset(void);
void MockNativeXGLGetStats(struct MockNativeXGLStats *);
void MockNativeXGLFailNextCreation(int object_type);
/* Fail the coverage-Z read following a native RGB readback. */
void MockNativeXGLFailAfterColorReadback(void);

#endif

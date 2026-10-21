# Mesa 4.0.4 SGI GLU 1.3 sources. C++ is used by the complete NURBS engine.
GLU_C_SOURCES = \
    vendor/glu/src/libutil/error.c \
    vendor/glu/src/libutil/glue.c \
    vendor/glu/src/libutil/mipmap.c \
    vendor/glu/src/libutil/project.c \
    vendor/glu/src/libutil/quad.c \
    vendor/glu/src/libutil/registry.c \
    vendor/glu/src/libtess/dict.c \
    vendor/glu/src/libtess/geom.c \
    vendor/glu/src/libtess/memalloc.c \
    vendor/glu/src/libtess/mesh.c \
    vendor/glu/src/libtess/normal.c \
    vendor/glu/src/libtess/priorityq.c \
    vendor/glu/src/libtess/render.c \
    vendor/glu/src/libtess/sweep.c \
    vendor/glu/src/libtess/tess.c \
    vendor/glu/src/libtess/tessmono.c

GLU_CXX_SOURCES = \
    vendor/glu/src/libnurbs/interface/bezierEval.cc \
    vendor/glu/src/libnurbs/interface/bezierPatch.cc \
    vendor/glu/src/libnurbs/interface/bezierPatchMesh.cc \
    vendor/glu/src/libnurbs/interface/glcurveval.cc \
    vendor/glu/src/libnurbs/interface/glinterface.cc \
    vendor/glu/src/libnurbs/interface/glrenderer.cc \
    vendor/glu/src/libnurbs/interface/glsurfeval.cc \
    vendor/glu/src/libnurbs/interface/incurveeval.cc \
    vendor/glu/src/libnurbs/interface/insurfeval.cc \
    vendor/glu/src/libnurbs/internals/arc.cc \
    vendor/glu/src/libnurbs/internals/arcsorter.cc \
    vendor/glu/src/libnurbs/internals/arctess.cc \
    vendor/glu/src/libnurbs/internals/backend.cc \
    vendor/glu/src/libnurbs/internals/basiccrveval.cc \
    vendor/glu/src/libnurbs/internals/basicsurfeval.cc \
    vendor/glu/src/libnurbs/internals/bin.cc \
    vendor/glu/src/libnurbs/internals/bufpool.cc \
    vendor/glu/src/libnurbs/internals/cachingeval.cc \
    vendor/glu/src/libnurbs/internals/ccw.cc \
    vendor/glu/src/libnurbs/internals/coveandtiler.cc \
    vendor/glu/src/libnurbs/internals/curve.cc \
    vendor/glu/src/libnurbs/internals/curvelist.cc \
    vendor/glu/src/libnurbs/internals/curvesub.cc \
    vendor/glu/src/libnurbs/internals/dataTransform.cc \
    vendor/glu/src/libnurbs/internals/displaylist.cc \
    vendor/glu/src/libnurbs/internals/flist.cc \
    vendor/glu/src/libnurbs/internals/flistsorter.cc \
    vendor/glu/src/libnurbs/internals/hull.cc \
    vendor/glu/src/libnurbs/internals/intersect.cc \
    vendor/glu/src/libnurbs/internals/knotvector.cc \
    vendor/glu/src/libnurbs/internals/mapdesc.cc \
    vendor/glu/src/libnurbs/internals/mapdescv.cc \
    vendor/glu/src/libnurbs/internals/maplist.cc \
    vendor/glu/src/libnurbs/internals/mesher.cc \
    vendor/glu/src/libnurbs/internals/monoTriangulationBackend.cc \
    vendor/glu/src/libnurbs/internals/monotonizer.cc \
    vendor/glu/src/libnurbs/internals/mycode.cc \
    vendor/glu/src/libnurbs/internals/nurbsinterfac.cc \
    vendor/glu/src/libnurbs/internals/nurbstess.cc \
    vendor/glu/src/libnurbs/internals/patch.cc \
    vendor/glu/src/libnurbs/internals/patchlist.cc \
    vendor/glu/src/libnurbs/internals/quilt.cc \
    vendor/glu/src/libnurbs/internals/reader.cc \
    vendor/glu/src/libnurbs/internals/renderhints.cc \
    vendor/glu/src/libnurbs/internals/slicer.cc \
    vendor/glu/src/libnurbs/internals/sorter.cc \
    vendor/glu/src/libnurbs/internals/splitarcs.cc \
    vendor/glu/src/libnurbs/internals/subdivider.cc \
    vendor/glu/src/libnurbs/internals/tobezier.cc \
    vendor/glu/src/libnurbs/internals/trimline.cc \
    vendor/glu/src/libnurbs/internals/trimregion.cc \
    vendor/glu/src/libnurbs/internals/trimvertpool.cc \
    vendor/glu/src/libnurbs/internals/uarray.cc \
    vendor/glu/src/libnurbs/internals/varray.cc \
    vendor/glu/src/libnurbs/nurbtess/directedLine.cc \
    vendor/glu/src/libnurbs/nurbtess/gridWrap.cc \
    vendor/glu/src/libnurbs/nurbtess/monoChain.cc \
    vendor/glu/src/libnurbs/nurbtess/monoPolyPart.cc \
    vendor/glu/src/libnurbs/nurbtess/monoTriangulation.cc \
    vendor/glu/src/libnurbs/nurbtess/partitionX.cc \
    vendor/glu/src/libnurbs/nurbtess/partitionY.cc \
    vendor/glu/src/libnurbs/nurbtess/polyDBG.cc \
    vendor/glu/src/libnurbs/nurbtess/polyUtil.cc \
    vendor/glu/src/libnurbs/nurbtess/primitiveStream.cc \
    vendor/glu/src/libnurbs/nurbtess/quicksort.cc \
    vendor/glu/src/libnurbs/nurbtess/rectBlock.cc \
    vendor/glu/src/libnurbs/nurbtess/sampleComp.cc \
    vendor/glu/src/libnurbs/nurbtess/sampleCompBot.cc \
    vendor/glu/src/libnurbs/nurbtess/sampleCompRight.cc \
    vendor/glu/src/libnurbs/nurbtess/sampleCompTop.cc \
    vendor/glu/src/libnurbs/nurbtess/sampleMonoPoly.cc \
    vendor/glu/src/libnurbs/nurbtess/sampledLine.cc \
    vendor/glu/src/libnurbs/nurbtess/searchTree.cc

GLU_HEADERS = \
    vendor/glu/src/include/gluos.h \
    vendor/glu/src/libnurbs/interface/bezierEval.h \
    vendor/glu/src/libnurbs/interface/bezierPatch.h \
    vendor/glu/src/libnurbs/interface/bezierPatchMesh.h \
    vendor/glu/src/libnurbs/interface/glcurveval.h \
    vendor/glu/src/libnurbs/interface/glimports.h \
    vendor/glu/src/libnurbs/interface/glrenderer.h \
    vendor/glu/src/libnurbs/interface/glsurfeval.h \
    vendor/glu/src/libnurbs/interface/mystdio.h \
    vendor/glu/src/libnurbs/interface/mystdlib.h \
    vendor/glu/src/libnurbs/internals/arc.h \
    vendor/glu/src/libnurbs/internals/arcsorter.h \
    vendor/glu/src/libnurbs/internals/arctess.h \
    vendor/glu/src/libnurbs/internals/backend.h \
    vendor/glu/src/libnurbs/internals/basiccrveval.h \
    vendor/glu/src/libnurbs/internals/basicsurfeval.h \
    vendor/glu/src/libnurbs/internals/bezierarc.h \
    vendor/glu/src/libnurbs/internals/bin.h \
    vendor/glu/src/libnurbs/internals/bufpool.h \
    vendor/glu/src/libnurbs/internals/cachingeval.h \
    vendor/glu/src/libnurbs/internals/coveandtiler.h \
    vendor/glu/src/libnurbs/internals/curve.h \
    vendor/glu/src/libnurbs/internals/curvelist.h \
    vendor/glu/src/libnurbs/internals/dataTransform.h \
    vendor/glu/src/libnurbs/internals/defines.h \
    vendor/glu/src/libnurbs/internals/displaylist.h \
    vendor/glu/src/libnurbs/internals/displaymode.h \
    vendor/glu/src/libnurbs/internals/flist.h \
    vendor/glu/src/libnurbs/internals/flistsorter.h \
    vendor/glu/src/libnurbs/internals/gridline.h \
    vendor/glu/src/libnurbs/internals/gridtrimvertex.h \
    vendor/glu/src/libnurbs/internals/gridvertex.h \
    vendor/glu/src/libnurbs/internals/hull.h \
    vendor/glu/src/libnurbs/internals/jarcloc.h \
    vendor/glu/src/libnurbs/internals/knotvector.h \
    vendor/glu/src/libnurbs/internals/mapdesc.h \
    vendor/glu/src/libnurbs/internals/maplist.h \
    vendor/glu/src/libnurbs/internals/mesher.h \
    vendor/glu/src/libnurbs/internals/monotonizer.h \
    vendor/glu/src/libnurbs/internals/myassert.h \
    vendor/glu/src/libnurbs/internals/mymath.h \
    vendor/glu/src/libnurbs/internals/mysetjmp.h \
    vendor/glu/src/libnurbs/internals/mystring.h \
    vendor/glu/src/libnurbs/internals/nurbsconsts.h \
    vendor/glu/src/libnurbs/internals/nurbstess.h \
    vendor/glu/src/libnurbs/internals/patch.h \
    vendor/glu/src/libnurbs/internals/patchlist.h \
    vendor/glu/src/libnurbs/internals/pwlarc.h \
    vendor/glu/src/libnurbs/internals/quilt.h \
    vendor/glu/src/libnurbs/internals/reader.h \
    vendor/glu/src/libnurbs/internals/renderhints.h \
    vendor/glu/src/libnurbs/internals/simplemath.h \
    vendor/glu/src/libnurbs/internals/slicer.h \
    vendor/glu/src/libnurbs/internals/sorter.h \
    vendor/glu/src/libnurbs/internals/subdivider.h \
    vendor/glu/src/libnurbs/internals/trimline.h \
    vendor/glu/src/libnurbs/internals/trimregion.h \
    vendor/glu/src/libnurbs/internals/trimvertex.h \
    vendor/glu/src/libnurbs/internals/trimvertpool.h \
    vendor/glu/src/libnurbs/internals/types.h \
    vendor/glu/src/libnurbs/internals/uarray.h \
    vendor/glu/src/libnurbs/internals/varray.h \
    vendor/glu/src/libnurbs/nurbtess/definitions.h \
    vendor/glu/src/libnurbs/nurbtess/directedLine.h \
    vendor/glu/src/libnurbs/nurbtess/glimports.h \
    vendor/glu/src/libnurbs/nurbtess/gridWrap.h \
    vendor/glu/src/libnurbs/nurbtess/monoChain.h \
    vendor/glu/src/libnurbs/nurbtess/monoPolyPart.h \
    vendor/glu/src/libnurbs/nurbtess/monoTriangulation.h \
    vendor/glu/src/libnurbs/nurbtess/mystdio.h \
    vendor/glu/src/libnurbs/nurbtess/mystdlib.h \
    vendor/glu/src/libnurbs/nurbtess/partitionX.h \
    vendor/glu/src/libnurbs/nurbtess/partitionY.h \
    vendor/glu/src/libnurbs/nurbtess/polyDBG.h \
    vendor/glu/src/libnurbs/nurbtess/polyUtil.h \
    vendor/glu/src/libnurbs/nurbtess/primitiveStream.h \
    vendor/glu/src/libnurbs/nurbtess/quicksort.h \
    vendor/glu/src/libnurbs/nurbtess/rectBlock.h \
    vendor/glu/src/libnurbs/nurbtess/sampleComp.h \
    vendor/glu/src/libnurbs/nurbtess/sampleCompBot.h \
    vendor/glu/src/libnurbs/nurbtess/sampleCompRight.h \
    vendor/glu/src/libnurbs/nurbtess/sampleCompTop.h \
    vendor/glu/src/libnurbs/nurbtess/sampleMonoPoly.h \
    vendor/glu/src/libnurbs/nurbtess/sampledLine.h \
    vendor/glu/src/libnurbs/nurbtess/searchTree.h \
    vendor/glu/src/libnurbs/nurbtess/zlassert.h \
    vendor/glu/src/libtess/dict-list.h \
    vendor/glu/src/libtess/dict.h \
    vendor/glu/src/libtess/geom.h \
    vendor/glu/src/libtess/memalloc.h \
    vendor/glu/src/libtess/mesh.h \
    vendor/glu/src/libtess/normal.h \
    vendor/glu/src/libtess/priorityq-heap.h \
    vendor/glu/src/libtess/priorityq-sort.h \
    vendor/glu/src/libtess/priorityq.h \
    vendor/glu/src/libtess/render.h \
    vendor/glu/src/libtess/sweep.h \
    vendor/glu/src/libtess/tess.h \
    vendor/glu/src/libtess/tessmono.h \
    vendor/glu/src/libutil/gluint.h \
    vendor/glu/include/GL/glu.h \
    include/GL/glu.h

GLU_C_OBJECTS = $(GLU_C_SOURCES:.c=.o)
GLU_CXX_OBJECTS = $(GLU_CXX_SOURCES:.cc=.o)
GLU_OBJECTS = $(GLU_C_OBJECTS) $(GLU_CXX_OBJECTS)
GLU_INCLUDES = -Iinclude -Ivendor/glu/src/include -Ivendor/glu/src/libutil -Ivendor/glu/src/libnurbs/internals -Ivendor/glu/src/libnurbs/interface -Ivendor/glu/src/libnurbs/nurbtess

glu: lib/libGLU.a lib/libGLU.so

$(GLU_C_OBJECTS): $(GLU_HEADERS) include/GL/gl.h
$(GLU_CXX_OBJECTS): $(GLU_HEADERS) include/GL/gl.h

vendor/glu/src/libutil/error.o: vendor/glu/src/libutil/error.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libutil/error.c -o $@

vendor/glu/src/libutil/glue.o: vendor/glu/src/libutil/glue.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libutil/glue.c -o $@

vendor/glu/src/libutil/mipmap.o: vendor/glu/src/libutil/mipmap.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libutil/mipmap.c -o $@

vendor/glu/src/libutil/project.o: vendor/glu/src/libutil/project.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libutil/project.c -o $@

vendor/glu/src/libutil/quad.o: vendor/glu/src/libutil/quad.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libutil/quad.c -o $@

vendor/glu/src/libutil/registry.o: vendor/glu/src/libutil/registry.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libutil/registry.c -o $@

vendor/glu/src/libtess/dict.o: vendor/glu/src/libtess/dict.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/dict.c -o $@

vendor/glu/src/libtess/geom.o: vendor/glu/src/libtess/geom.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/geom.c -o $@

vendor/glu/src/libtess/memalloc.o: vendor/glu/src/libtess/memalloc.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/memalloc.c -o $@

vendor/glu/src/libtess/mesh.o: vendor/glu/src/libtess/mesh.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/mesh.c -o $@

vendor/glu/src/libtess/normal.o: vendor/glu/src/libtess/normal.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/normal.c -o $@

vendor/glu/src/libtess/priorityq.o: vendor/glu/src/libtess/priorityq.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/priorityq.c -o $@

vendor/glu/src/libtess/render.o: vendor/glu/src/libtess/render.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/render.c -o $@

vendor/glu/src/libtess/sweep.o: vendor/glu/src/libtess/sweep.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/sweep.c -o $@

vendor/glu/src/libtess/tess.o: vendor/glu/src/libtess/tess.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/tess.c -o $@

vendor/glu/src/libtess/tessmono.o: vendor/glu/src/libtess/tessmono.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libtess/tessmono.c -o $@

vendor/glu/src/libnurbs/interface/bezierEval.o: vendor/glu/src/libnurbs/interface/bezierEval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/bezierEval.cc -o $@

vendor/glu/src/libnurbs/interface/bezierPatch.o: vendor/glu/src/libnurbs/interface/bezierPatch.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/bezierPatch.cc -o $@

vendor/glu/src/libnurbs/interface/bezierPatchMesh.o: vendor/glu/src/libnurbs/interface/bezierPatchMesh.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/bezierPatchMesh.cc -o $@

vendor/glu/src/libnurbs/interface/glcurveval.o: vendor/glu/src/libnurbs/interface/glcurveval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/glcurveval.cc -o $@

vendor/glu/src/libnurbs/interface/glinterface.o: vendor/glu/src/libnurbs/interface/glinterface.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/glinterface.cc -o $@

vendor/glu/src/libnurbs/interface/glrenderer.o: vendor/glu/src/libnurbs/interface/glrenderer.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/glrenderer.cc -o $@

vendor/glu/src/libnurbs/interface/glsurfeval.o: vendor/glu/src/libnurbs/interface/glsurfeval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/glsurfeval.cc -o $@

vendor/glu/src/libnurbs/interface/incurveeval.o: vendor/glu/src/libnurbs/interface/incurveeval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/incurveeval.cc -o $@

vendor/glu/src/libnurbs/interface/insurfeval.o: vendor/glu/src/libnurbs/interface/insurfeval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/interface/insurfeval.cc -o $@

vendor/glu/src/libnurbs/internals/arc.o: vendor/glu/src/libnurbs/internals/arc.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/arc.cc -o $@

vendor/glu/src/libnurbs/internals/arcsorter.o: vendor/glu/src/libnurbs/internals/arcsorter.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/arcsorter.cc -o $@

vendor/glu/src/libnurbs/internals/arctess.o: vendor/glu/src/libnurbs/internals/arctess.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/arctess.cc -o $@

vendor/glu/src/libnurbs/internals/backend.o: vendor/glu/src/libnurbs/internals/backend.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/backend.cc -o $@

vendor/glu/src/libnurbs/internals/basiccrveval.o: vendor/glu/src/libnurbs/internals/basiccrveval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/basiccrveval.cc -o $@

vendor/glu/src/libnurbs/internals/basicsurfeval.o: vendor/glu/src/libnurbs/internals/basicsurfeval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/basicsurfeval.cc -o $@

vendor/glu/src/libnurbs/internals/bin.o: vendor/glu/src/libnurbs/internals/bin.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/bin.cc -o $@

vendor/glu/src/libnurbs/internals/bufpool.o: vendor/glu/src/libnurbs/internals/bufpool.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/bufpool.cc -o $@

vendor/glu/src/libnurbs/internals/cachingeval.o: vendor/glu/src/libnurbs/internals/cachingeval.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/cachingeval.cc -o $@

vendor/glu/src/libnurbs/internals/ccw.o: vendor/glu/src/libnurbs/internals/ccw.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/ccw.cc -o $@

vendor/glu/src/libnurbs/internals/coveandtiler.o: vendor/glu/src/libnurbs/internals/coveandtiler.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/coveandtiler.cc -o $@

vendor/glu/src/libnurbs/internals/curve.o: vendor/glu/src/libnurbs/internals/curve.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/curve.cc -o $@

vendor/glu/src/libnurbs/internals/curvelist.o: vendor/glu/src/libnurbs/internals/curvelist.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/curvelist.cc -o $@

vendor/glu/src/libnurbs/internals/curvesub.o: vendor/glu/src/libnurbs/internals/curvesub.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/curvesub.cc -o $@

vendor/glu/src/libnurbs/internals/dataTransform.o: vendor/glu/src/libnurbs/internals/dataTransform.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/dataTransform.cc -o $@

vendor/glu/src/libnurbs/internals/displaylist.o: vendor/glu/src/libnurbs/internals/displaylist.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/displaylist.cc -o $@

vendor/glu/src/libnurbs/internals/flist.o: vendor/glu/src/libnurbs/internals/flist.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/flist.cc -o $@

vendor/glu/src/libnurbs/internals/flistsorter.o: vendor/glu/src/libnurbs/internals/flistsorter.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/flistsorter.cc -o $@

vendor/glu/src/libnurbs/internals/hull.o: vendor/glu/src/libnurbs/internals/hull.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/hull.cc -o $@

vendor/glu/src/libnurbs/internals/intersect.o: vendor/glu/src/libnurbs/internals/intersect.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/intersect.cc -o $@

vendor/glu/src/libnurbs/internals/knotvector.o: vendor/glu/src/libnurbs/internals/knotvector.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/knotvector.cc -o $@

vendor/glu/src/libnurbs/internals/mapdesc.o: vendor/glu/src/libnurbs/internals/mapdesc.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/mapdesc.cc -o $@

vendor/glu/src/libnurbs/internals/mapdescv.o: vendor/glu/src/libnurbs/internals/mapdescv.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/mapdescv.cc -o $@

vendor/glu/src/libnurbs/internals/maplist.o: vendor/glu/src/libnurbs/internals/maplist.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/maplist.cc -o $@

vendor/glu/src/libnurbs/internals/mesher.o: vendor/glu/src/libnurbs/internals/mesher.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/mesher.cc -o $@

vendor/glu/src/libnurbs/internals/monoTriangulationBackend.o: vendor/glu/src/libnurbs/internals/monoTriangulationBackend.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/monoTriangulationBackend.cc -o $@

vendor/glu/src/libnurbs/internals/monotonizer.o: vendor/glu/src/libnurbs/internals/monotonizer.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/monotonizer.cc -o $@

vendor/glu/src/libnurbs/internals/mycode.o: vendor/glu/src/libnurbs/internals/mycode.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/mycode.cc -o $@

vendor/glu/src/libnurbs/internals/nurbsinterfac.o: vendor/glu/src/libnurbs/internals/nurbsinterfac.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/nurbsinterfac.cc -o $@

vendor/glu/src/libnurbs/internals/nurbstess.o: vendor/glu/src/libnurbs/internals/nurbstess.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/nurbstess.cc -o $@

vendor/glu/src/libnurbs/internals/patch.o: vendor/glu/src/libnurbs/internals/patch.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/patch.cc -o $@

vendor/glu/src/libnurbs/internals/patchlist.o: vendor/glu/src/libnurbs/internals/patchlist.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/patchlist.cc -o $@

vendor/glu/src/libnurbs/internals/quilt.o: vendor/glu/src/libnurbs/internals/quilt.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/quilt.cc -o $@

vendor/glu/src/libnurbs/internals/reader.o: vendor/glu/src/libnurbs/internals/reader.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/reader.cc -o $@

vendor/glu/src/libnurbs/internals/renderhints.o: vendor/glu/src/libnurbs/internals/renderhints.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/renderhints.cc -o $@

vendor/glu/src/libnurbs/internals/slicer.o: vendor/glu/src/libnurbs/internals/slicer.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/slicer.cc -o $@

vendor/glu/src/libnurbs/internals/sorter.o: vendor/glu/src/libnurbs/internals/sorter.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/sorter.cc -o $@

vendor/glu/src/libnurbs/internals/splitarcs.o: vendor/glu/src/libnurbs/internals/splitarcs.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/splitarcs.cc -o $@

vendor/glu/src/libnurbs/internals/subdivider.o: vendor/glu/src/libnurbs/internals/subdivider.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/subdivider.cc -o $@

vendor/glu/src/libnurbs/internals/tobezier.o: vendor/glu/src/libnurbs/internals/tobezier.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/tobezier.cc -o $@

vendor/glu/src/libnurbs/internals/trimline.o: vendor/glu/src/libnurbs/internals/trimline.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/trimline.cc -o $@

vendor/glu/src/libnurbs/internals/trimregion.o: vendor/glu/src/libnurbs/internals/trimregion.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/trimregion.cc -o $@

vendor/glu/src/libnurbs/internals/trimvertpool.o: vendor/glu/src/libnurbs/internals/trimvertpool.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/trimvertpool.cc -o $@

vendor/glu/src/libnurbs/internals/uarray.o: vendor/glu/src/libnurbs/internals/uarray.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/uarray.cc -o $@

vendor/glu/src/libnurbs/internals/varray.o: vendor/glu/src/libnurbs/internals/varray.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/internals/varray.cc -o $@

vendor/glu/src/libnurbs/nurbtess/directedLine.o: vendor/glu/src/libnurbs/nurbtess/directedLine.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/directedLine.cc -o $@

vendor/glu/src/libnurbs/nurbtess/gridWrap.o: vendor/glu/src/libnurbs/nurbtess/gridWrap.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/gridWrap.cc -o $@

vendor/glu/src/libnurbs/nurbtess/monoChain.o: vendor/glu/src/libnurbs/nurbtess/monoChain.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/monoChain.cc -o $@

vendor/glu/src/libnurbs/nurbtess/monoPolyPart.o: vendor/glu/src/libnurbs/nurbtess/monoPolyPart.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/monoPolyPart.cc -o $@

vendor/glu/src/libnurbs/nurbtess/monoTriangulation.o: vendor/glu/src/libnurbs/nurbtess/monoTriangulation.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/monoTriangulation.cc -o $@

vendor/glu/src/libnurbs/nurbtess/partitionX.o: vendor/glu/src/libnurbs/nurbtess/partitionX.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/partitionX.cc -o $@

vendor/glu/src/libnurbs/nurbtess/partitionY.o: vendor/glu/src/libnurbs/nurbtess/partitionY.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/partitionY.cc -o $@

vendor/glu/src/libnurbs/nurbtess/polyDBG.o: vendor/glu/src/libnurbs/nurbtess/polyDBG.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/polyDBG.cc -o $@

vendor/glu/src/libnurbs/nurbtess/polyUtil.o: vendor/glu/src/libnurbs/nurbtess/polyUtil.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/polyUtil.cc -o $@

vendor/glu/src/libnurbs/nurbtess/primitiveStream.o: vendor/glu/src/libnurbs/nurbtess/primitiveStream.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/primitiveStream.cc -o $@

vendor/glu/src/libnurbs/nurbtess/quicksort.o: vendor/glu/src/libnurbs/nurbtess/quicksort.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/quicksort.cc -o $@

vendor/glu/src/libnurbs/nurbtess/rectBlock.o: vendor/glu/src/libnurbs/nurbtess/rectBlock.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/rectBlock.cc -o $@

vendor/glu/src/libnurbs/nurbtess/sampleComp.o: vendor/glu/src/libnurbs/nurbtess/sampleComp.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/sampleComp.cc -o $@

vendor/glu/src/libnurbs/nurbtess/sampleCompBot.o: vendor/glu/src/libnurbs/nurbtess/sampleCompBot.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/sampleCompBot.cc -o $@

vendor/glu/src/libnurbs/nurbtess/sampleCompRight.o: vendor/glu/src/libnurbs/nurbtess/sampleCompRight.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/sampleCompRight.cc -o $@

vendor/glu/src/libnurbs/nurbtess/sampleCompTop.o: vendor/glu/src/libnurbs/nurbtess/sampleCompTop.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/sampleCompTop.cc -o $@

vendor/glu/src/libnurbs/nurbtess/sampleMonoPoly.o: vendor/glu/src/libnurbs/nurbtess/sampleMonoPoly.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/sampleMonoPoly.cc -o $@

vendor/glu/src/libnurbs/nurbtess/sampledLine.o: vendor/glu/src/libnurbs/nurbtess/sampledLine.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/sampledLine.cc -o $@

vendor/glu/src/libnurbs/nurbtess/searchTree.o: vendor/glu/src/libnurbs/nurbtess/searchTree.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(GLU_INCLUDES) -DNDEBUG -DLIBRARYBUILD -c vendor/glu/src/libnurbs/nurbtess/searchTree.cc -o $@

lib/libGLU.a: $(GLU_OBJECTS)
	mkdir -p lib
	$(AR) rcs $@ $(GLU_OBJECTS)

lib/libGLU.so: $(GLU_OBJECTS) lib/libGL.so
	mkdir -p lib
	$(CXX) $(SHARED_FLAGS) $(LDFLAGS) -o $@ $(GLU_OBJECTS) -Llib -lGL $(LDLIBS)

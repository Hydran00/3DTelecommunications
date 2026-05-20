#include "CudaTextureHandles.h"

// Define device-side texture/surface symbols exactly once
__device__ cudaSurfaceObject_t surf_visHull_dev = 0;
__device__ cudaTextureObject_t tex_visHull_dev = 0;
__device__ cudaTextureObject_t tex_depthImgs_dev = 0;
__device__ cudaTextureObject_t tex_normalMaps_dev = 0;

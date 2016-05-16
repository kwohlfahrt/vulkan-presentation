# Vulkan Presentaion

A series of small programs to generate TIFF images to be used in my
presentation. Covers different pipeline stages.

## Installation

### Requirements

- libtiff
- vulkan
- glslang

### Generating Images

Just run `make all`.

## Slide Overview

### Device Coords

Shows device coords, with triangle points at multiples of 0.5 in x and y.

### Vertex Shader

Shows transformation of vertices in position, as well as adding of vertex
properties, in this case color.

### Rasterization

Shows line distance equation can be evaluated to determine which sample
to rasterize.

### Fill

Shows interpolation of point attributes.

### Texture

Shows per-fragment processing, in this case fetching texture value.

### Blend

Shows different post-fragment processing: Depth-test (technically pre-fragment,
but conceptually not) and additive blending.

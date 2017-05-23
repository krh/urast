Introduction
------------

urast (micro rasterizer) is a tiny triangle rasterizer. It takes a
triangle list as input and rasterizes the triangles in random shades
of red.

Building
--------

urast uses the meson build system. To build and run the example, do
```
mkdir build
meson . build
ninja-build -C build
./build/urast
```

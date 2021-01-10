# spath
Simple path tracer

![spath in action](https://raw.githubusercontent.com/Emanem/spath/master/imgs/spath.png)

## Summary
Very simple program to execute basic (Lambertian) _path tracing_ on triangles.
I've developed this just to play a bit, this is indeed experimental.

## Rationale
Purpose of this application is to test and form basic knowledge of _path-tracing_ (the simple bit of code) and then via _OpenCL_ and _Vulkan (compute)_.
Furthermore I would want to also integrate the _RT acceleration_ Vulkan API (once gets available in mainline Linux Nvidia stable drivers).

## How this works
This application is really simple: it creates a scene with triangles and then allows you to move in it, recalculating every frame from scratch.
The are no optimizations with regards to buffers management, and I'm using _OpenGL_ to display the _X window_: basically each frame is computed on the
GPU, then shipped to RAM, then copied over to a _GL_ buffer and finally displayed.
Should have used direct _OpenCL_ or _Vulkan_ display extensions/features, but this is an area I don't particularly care to optimize.
Currently the _path-traced_ bounces per ray are limited to 5.

## Limitations

* The current renderers (both CPU and GPUs - _OpenCL_ and _Vulkan_) do not have any ray-triangle intersection optimization (i.e. [BVH](https://en.wikipedia.org/wiki/Bounding_volume_hierarchy)) and do a brute force linear scan through all triangles in the scene
* All the triangles are _flat_ (i.e. one _normal_ per triangle)
* No reflective materials
* No transparency

## How to build
You will need _freeglut_, _OpenCL_ and _Vulkan dev_ installed to compile:
```sudo apt install freeglut3-dev opencl-headers opencl-clhpp-headers libvulkan-dev```.

You would also need _Vulkan Hpp_ header file, if it doesn't come with _Vulkan dev_ you may download from 
[github](https://github.com/KhronosGroup/Vulkan-Hpp/blob/master/vulkan/vulkan.hpp) and copy to ```/usr/include/vulkan/vulkan.hpp```.

If your _Vulkan headers_ are going to be out of date (build error) the best next mvoe is to install them from [LunarXchange](https://vulkan.lunarg.com/sdk/home#linux) (for Ubuntu they have a nice ppa which would do for you); don't forget to ```sudo apt install vulkan-headers``` in this case. You need at least version `1.2.162`.

On Nvidia, one also has to create a _symlink_ to the _OpenCL.so_ library (not done by the installation of dev packages by default); in order
to achieve this, let's add the following symlink ```sudo ln -s /usr/lib/x86_64-linux-gnu/libOpenCL.so.1 /usr/lib/libOpenCL.so```.
Another way would be to install the icd via ```sudo apt install ocl-icd-opencl-dev```.

In case of AMD, don't forget to install the open source driver ```sudo apt install mesa-opencl-icd```.
 
Once done, then just invoke `make` or `make release` and the executable should be compiled.

## How to use
This doesn't do much, just displays the scene on a GL window.
By default it executes flat shading (as a debug tool). Use following keys to control a bit:

Key | Action
----|-------
w   | Move forward
s   | Move backward
a   | Strafe left
d   | Strafe right
f   | Increase focal (i.e. zoom in)
g   | Decrease focal (i.e. zoom out)
r   | Switches renderer (_CPU_ to _OpenCL_ to _Vulkan_)
p   | Switches tracing (_flat_ to _path tracing_)
\+   | Increases samples (twice)
\-   | Decreases samples (halves)
q   | Quits
Esc | Quits
MLB | Rotate horizontally/vertically when dragging the mouse


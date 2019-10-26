# spath
Simple path tracer

## Summary
Very simple program to execute basic (Lambertian) _path tracing_ on triangles.
I've developed this just to play a bit, this is indeed experimental.

## How to build
You will need _freeglut_ installed to compile:
```sudo apt install freeglut3-dev```
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
q   | Rotate left
e   | Rotate right
f   | Increase focal (i.e. zoom in)
g   | Decrease focal (i.e. zoom out)
r   | Switches from _flat_ to _path tracing_
+   | Increases samples (twice)
-   | Decreases samples (halves)
Esc | Quits


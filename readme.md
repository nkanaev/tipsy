# tipsy

  *tipsy* (tiny playstationy) is a PS1-like software renderer written in ~500 lines of C99.\
  Features all the charm of the original PS1, like:
  * low resolution display (320x240)
  * lack of anti-aliasing (pixelated textures)
  * affine texture mapping (warped textures)
  * lack of subpixel rasterization (polygon jittering/wobble)
  * old shading techniques (flat & Gouraud)

> Whatever you now find weird, ugly, uncomfortable and nasty about a new medium
> will surely become its signature. CD distortion, the jitteriness of
> digital video, the crap sound of 8-bit - all of these will be cherished
> and emulated as soon as they can be avoided...
>
> ― Brian Eno, A Year With Swollen Appendices

## building

  Building requires C99 compiler & OpenGL (see `makefile`).

  The project should compile on:
  * Linux (tested on Ubuntu 20.04)
  * MacOS (tested on Monterey)
  * Windows (untested)

  Get the source & run:

    make

## usage

    ./tipsy path/to/wavefront.obj

  Hold down the left mouse button and drag to rotate.

  Keybindings:

  * <kbd>Left</kbd>/<kbd>Right</kbd>:
    rotate horizontally
  * <kbd>Up</kbd>/<kbd>Down</kbd>:
    rotate vertically
  * <kbd>W</kbd>:
    toggle wireframe drawing
  * <kbd>Z</kbd>:
    toggle z-buffering
  * <kbd>P</kbd>:
    toggle perspective correct texture mapping (default = off)
  * <kbd>C</kbd>:
    toggle back/front face culling (default = back)
  * <kbd>J</kbd>:
    toggle jittering (default = on)
  * <kbd>F</kbd>:
    toggle vertical flip
  * <kbd>R</kbd>:
    reset model position
  * <kbd>1</kbd>:
    switch off shading (default)
  * <kbd>2</kbd>:
    switch to flat shading
  * <kbd>3</kbd>:
    switch to gouraud shading

## credits

* [Building a PS1 style retro 3D renderer](https://www.david-colson.com/2021/11/30/ps1-style-renderer.html)
  article by David Colson. \
  Polygon jittering imitation technique was taken from there.
* [Playstation Architecture](https://www.copetti.org/writings/consoles/playstation/)
  article by Rodrigo Copetti. \
  Provides a good overview of the PSX architecture.
* [gel](https://github.com/glouw/gel)
  by Gustav Louw. Similar project with the same spirit.

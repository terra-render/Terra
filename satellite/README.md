# Satellite
Front-end client for [Terra](https://github.com/c4stan/Terra).    

It is used as development tool for `Terra`, where functionality is exposed through an in-app console.  

A human-made Visual Studio solution can be found `vs`. It doesn't link to any library, but references the sources and compiles everything together.   
The solution is already setup to look for the sources in the `dependencies` folder. Cloning with `--recurse-submodule` will work out of the box.  
You only need to run `python gl3w_gen.py` from inside `satellite/dependencies/gl3w` to generate the `gl3w.h` and `gl3w.c` source files.  


Dependencies:  
- [GLFW](http://www.glfw.org/)
- [gl3w](http://glew.sourceforge.net/)
- [imgui](https://github.com/ocornut/imgui/) 


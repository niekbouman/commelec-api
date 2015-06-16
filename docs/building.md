## Library and Compiler Dependencies
The daemon C++ code depends on the following libraries, which should be installed:
* [Cap&#39;n Proto](https://capnproto.org), >= 0.5.0
* [Eigen3](http://eigen.tuxfamily.org/), >= 3.0.0 (a header-only library)
* [Boost](http://www.boost.org), (required pre-compiled library components: system, filesystem, coroutine, context)

Furthermore, the project uses [RapidJSON](https://github.com/miloyip/rapidjson) (for dealing with JSON) and [spdlog](https://github.com/gabime/spdlog) for logging purposes, but these libraries are header-only and included as Git submodules in the project.

The code uses [C++11](http://en.wikipedia.org/wiki/C++11), hence needs a newer compiler (e.g., one of those listed below):
* GNU C/C++ compiler >= 4.8 (Linux)
* Clang >= 3.3 (MacOS, FreeBSD)
* Visual C++ >= 2015 (Windows)
* MinGW-w64 >= 3 (for cross-compiling to Windows)

## Cloning the repository including the submodules
To properly clone the Git repository including the submodules, issue
```
git clone --recursive https://github.com/niekbouman/commelec-api.git
```
If you have already cloned the repository without the `--recursive` flag, 
then you should cd into the directory, and initialize and update the submodules manually, i.e.,
```
cd commelec-api
git submodule init
git submodule update
```

## Building the project
For an out-of-source build, and if you have the [ninja](https://martine.github.io/ninja/) build system installed (which is recommended for speed), issue the following commands
```
cd commelec-api
mkdir build
cd build
cmake -G Ninja ..
ninja
```
If you do not have ninja installed, you can use
```
cd commelec-api
mkdir build
cd build
cmake ..
make -j6
```

If everything went well, there should now be an executable `commelecd` located in the `commelec-api/build/commelec-api` directory. 
Additionally, a static and a dynamic high-level API library should have been built (on Linux, these files are named `libhlapi_static.a` and `libhlapi.so`).

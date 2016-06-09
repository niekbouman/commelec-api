## Example: Building using Docker
The Dockerfile below shows how the project can be built under Debian unstable.

```
# Setup a debian (unstable) enviroment
FROM debian:unstable
RUN apt-get update && apt-get install -y \
    build-essential \
    capnproto \
    libcapnp-dev \
    cmake \
    git \
    libboost-context-dev \
    libboost-coroutine-dev \
    libboost-coroutine1.58.0 \
    libboost-system-dev \
    libboost-filesystem-dev \
    libboost-chrono-dev \
    libboost-dev \
    libeigen3-dev \
    ninja-build \
 && rm -rf /var/lib/apt/lists/*

# Clone the repo
RUN git clone --recursive https://github.com/niekbouman/commelec-api.git 

# Build the project
RUN cd commelec-api \
 && mkdir -p build \
 && cd build \
 && cmake -DCMAKE_BUILD_TYPE=Release -GNinja .. \
 && ninja
```

To use this Dockerfile, first make sure that you have installed [Docker](http://www.docker.com). Then, create a directory somewhere and put the above in a file that is named `Dockerfile`, or download the above as a file [here](../docker/Dockerfile). 

Launch a docker console, and change to the directory that contains the Dockerfile.
Then, issue

```
docker build -t capi --rm=true .
```
to build the image.

After the build completes, you can run the image and interact with it via a terminal, by issuing 
```
docker run -t -i capi /bin/bash
```

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

## Instructions for Installing the Required Dependencies on Debian/Ubuntu 
(Where we assume that your Linux distribution comes with g++ with version >=4.8)

```
sudo apt-get install build-essential git ninja-build cmake libboost-dev libboost-system-dev libboost-filesystem-dev libboost-coroutine-dev libboost-context-dev libeigen3-dev
```

We compile and install Cap'n Proto manually to ensure that the version is sufficiently recent (the instructions below originate from [the installation page of Cap'n Proto's website](https://capnproto.org/install.html): 
```
wget https://capnproto.org/capnproto-c++-0.5.2.tar.gz
tar zxf capnproto-c++-0.5.2.tar.gz
cd capnproto-c++-0.5.2
./configure
make -j6 check
sudo make install
```

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

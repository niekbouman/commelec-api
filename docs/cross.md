## Cross compilation for Zynq 7000-based CompactRIO (and other ARM) targets

We assume here the debian operating system.

Cross compiling the Commelec API for ARM consists of a number of steps
* Preparing Debian for cross compilation to armel
* Installing armel versions of Boost 
* Putting symlinks to libraries so that cmake can find them
* Cross-compiling the Capnproto library
* Cross-Building the project

## Preparing Debian for cross compilation to armel

Follow the [Debian Cross Toolchain instructions](https://wiki.debian.org/CrossToolchains) for setting up a cross build environment.
It is important to choose **armel** as the foreign architecture.

Depending on whether you run stable or unstable, you need to add the emdebian repository (see the Cross Toolcha instructions). Then:

```
sudo dpkg --add-architecture armel
sudo apt-get update
sudo apt-get install crossbuild-essential-armhf
```

## Installing armel versions of Boost 

Now install :armel versions of boost-context, boost-coroutine, boost-system, boost-filesystem and boost-chrono

## Putting symlinks to libraries so that cmake can find them

When cross-compiling with our toolchain file (explained further below), CMake will look in `/etc/arm-linux-gnueabi/include` and `/etc/arm-linux-gnueabi/lib` and for headers and libraries respectively. Hence, we will need to make some symlinks:

```
cd /usr/arm-linux-gnueabi
sudo mkdir -p include
sudo ln -s /usr/lib/arm-linux-gnueabi lib
cd include
sudo ln -s /usr/include/eigen3 eigen3
sudo ln -s /usr/include/boost boost
```

## Cross-compiling the Capnproto library

```
wget https://capnproto.org/capnproto-c++-0.5.3.tar.gz
tar zxf capnproto-c++-0.5.3.tar.gz
cd capnproto-c++-0.5.3
./configure --with-external-capnp --host=arm-linux-gnueabi --target=arm-linux-gnueabi
make -j6
```
Assuming that the build is successful, we now symlink the headers and copy the static libraries to our cross-build environment:
``` 
cd .libs
sudo cp *.a /usr/arm-linux-gnueabi/lib
cd /usr/arm-linux-gnueabi/include
sudo ln -s /usr/local/include/capnp capnp
sudo ln -s /usr/local/include/kj kj
```

## CrossBuilding the project

First we clone the project:
```
git clone --recursive https://github.com/niekbouman/commelec-api.git
```
Create build environment
```
cd commelec-api
mkdir arm_build
cd arm_build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../toolchains/linux-arm-toolchain.cmake .. 
```
At this step, CMake will complain if it cannot find the libraries.
If everthing goes well, you can build the project
```
ninja commelecd
```

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

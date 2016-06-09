set(HOST arm-linux-gnueabi)
set(CMAKE_SYSTEM_NAME Linux)

set(CMAKE_C_COMPILER "${HOST}-gcc-4.9")
set(CMAKE_CXX_COMPILER "${HOST}-g++-4.9")

# here is the target environment located
set(CMAKE_FIND_ROOT_PATH "/usr/${HOST}")
# here is where stuff gets installed to
set(CMAKE_INSTALL_PREFIX "${CMAKE_FIND_ROOT_PATH}/usr"
  CACHE STRING "Install-path prefix." FORCE)
# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment,
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
# set env vars so that pkg-config will look in the appropriate
# directory for .pc files (as there seems to be no way to
# force using ${HOST}-pkg-config)
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig")
set(ENV{PKG_CONFIG_PATH} "")

set(ADDITIONAL_CXX_FLAGS "-march=armv7-a -mtune=cortex-a9 -mfloat-abi=softfp -mfpu=neon")

# link statically to Cap'n Proto
set(CAPNP_STATIC ON)

# link statically to Boost
set(Boost_USE_STATIC_LIBS ON)


# Image with SDL 3 and ANGLE (OpenGL ES 3.0) for the native layer described
# in views/NATIVE.md, with Xvfb and VNC to run the views in a container (see
# views/VNC.md). Rendering is software (Mesa llvmpipe under ANGLE): no GPU is
# needed.
#
# Build, from this directory:
#   docker build -t skui-native .
# On top of another Ubuntu 24.04 based image (the Skip toolchain one, for
# example), keeping its user:
#   docker build --build-arg BASE=<image> --build-arg RUN_AS=<user> \
#     -t skui-native .
# Check it, in a container of the image:
#   xvfb-run -a /opt/sdl3/bin/skui-check-sdl-angle
#
# SDL 3 is built from source: Ubuntu 24.04 has no SDL 3 package. ANGLE comes
# from the prebuilt archives of github.com/kubuszok/sge-angle-natives.

ARG BASE=ubuntu:24.04

FROM ubuntu:24.04 AS sdl

ARG SDL_VERSION=3.4.20
ARG SDL_SHA256=f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f
ARG ANGLE_RELEASE=chromium-7151
ARG ANGLE_SHA256_X86_64=9828dd93937bae077d3459c02b7db27dae9465590dd0e22c4f8a2ab1ff7da33c
ARG ANGLE_SHA256_AARCH64=da9c2c2c90deaf0becd08f253ed51cdde689269f28443a857e2209b21a3b0bc2

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl build-essential cmake ninja-build pkg-config \
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev \
    libxi-dev libxss-dev libxtst-dev libxkbcommon-dev \
    libwayland-dev wayland-protocols libdecor-0-dev \
    libegl-dev libgles-dev libdrm-dev libgbm-dev \
    libdbus-1-dev libibus-1.0-dev libudev-dev \
    xvfb xauth libgl1-mesa-dri \
    && rm -rf /var/lib/apt/lists/*

# SDL 3, installed in /opt/sdl3. The video drivers (X11, Wayland, KMS/DRM)
# and input methods (IBus, Fcitx through D-Bus) are loaded at run time.
RUN curl -fsSL -o /tmp/sdl.tar.gz \
      https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz \
    && echo "${SDL_SHA256}  /tmp/sdl.tar.gz" | sha256sum -c - \
    && tar xzf /tmp/sdl.tar.gz -C /tmp \
    && cmake -S /tmp/SDL3-${SDL_VERSION} -B /tmp/sdl-build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/sdl3 \
      -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_TEST_LIBRARY=OFF \
    && ninja -C /tmp/sdl-build install \
    && rm -rf /tmp/sdl.tar.gz /tmp/SDL3-${SDL_VERSION} /tmp/sdl-build

# ANGLE (libEGL.so, libGLESv2.so and the EGL/GLES headers), in /opt/angle.
RUN arch="$(uname -m)" \
    && case "${arch}" in \
      x86_64) sha="${ANGLE_SHA256_X86_64}" ;; \
      aarch64) sha="${ANGLE_SHA256_AARCH64}" ;; \
      *) echo "no ANGLE build for ${arch}" && exit 1 ;; \
    esac \
    && curl -fsSL -o /tmp/angle.tar.gz \
      https://github.com/kubuszok/sge-angle-natives/releases/download/${ANGLE_RELEASE}/angle-linux-${arch}.tar.gz \
    && echo "${sha}  /tmp/angle.tar.gz" | sha256sum -c - \
    && mkdir -p /opt/angle \
    && tar xzf /tmp/angle.tar.gz -C /opt/angle \
    && rm /tmp/angle.tar.gz

# Fail the build if SDL 3 cannot open a window with an ANGLE OpenGL ES 3
# context.
COPY check.c /tmp/check.c
RUN mkdir -p /opt/sdl3/bin \
    && gcc -I/opt/sdl3/include -I/opt/angle/include /tmp/check.c \
      -L/opt/sdl3/lib -lSDL3 -Wl,-rpath,/opt/sdl3/lib -o /opt/sdl3/bin/skui-check-sdl-angle \
    && mkdir -p -m 700 /tmp/runtime \
    && XDG_RUNTIME_DIR=/tmp/runtime xvfb-run -a /opt/sdl3/bin/skui-check-sdl-angle

FROM ${BASE}

# The user the image runs as at the end (the base image may use a non-root
# user, while the installation below needs root).
ARG RUN_AS=root
USER root
ENV DEBIAN_FRONTEND=noninteractive
# Libraries SDL 3 and ANGLE load at run time, Mesa (llvmpipe) for ANGLE's
# OpenGL back end, the X11 headers SDL's headers include, and the virtual
# screen and VNC servers.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libx11-6 libxext6 libxrandr2 libxcursor1 libxfixes3 libxi6 libxss1 \
    libxtst6 libxkbcommon0 libwayland-client0 libwayland-cursor0 \
    libwayland-egl1 libdecor-0-0 libdbus-1-3 libibus-1.0-5 libudev1 \
    libegl1 libgl1 libgl1-mesa-dri libgbm1 \
    xvfb xauth x11vnc novnc websockify \
    && rm -rf /var/lib/apt/lists/*

COPY --from=sdl /opt/sdl3 /opt/sdl3
COPY --from=sdl /opt/angle /opt/angle

# SDL 3 is found by the dynamic linker and pkg-config (sdl3.pc). ANGLE is
# not added to the linker path: applications link it with -L/opt/angle/lib
# and an rpath, and SDL loads it through SDL_HINT_EGL_LIBRARY and
# SDL_HINT_OPENGL_LIBRARY.
RUN echo /opt/sdl3/lib > /etc/ld.so.conf.d/sdl3.conf && ldconfig
ENV PKG_CONFIG_PATH=/opt/sdl3/lib/pkgconfig:${PKG_CONFIG_PATH} \
    SKUI_ANGLE_DIR=/opt/angle
USER ${RUN_AS}

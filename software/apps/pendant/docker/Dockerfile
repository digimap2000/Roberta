FROM debian:trixie

ENV DEBIAN_FRONTEND=noninteractive
ENV PKG_CONFIG_DIR=
ENV PKG_CONFIG_PATH=
ENV PKG_CONFIG_LIBDIR=/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig:/usr/lib/pkgconfig
ENV PKG_CONFIG_SYSROOT_DIR=/

RUN dpkg --add-architecture arm64 \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        bash \
        ca-certificates \
        cmake \
        crossbuild-essential-arm64 \
        file \
        git \
        libegl1-mesa-dev:arm64 \
        libgles2-mesa-dev:arm64 \
        libsdl2-dev:arm64 \
        ninja-build \
        pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace/software

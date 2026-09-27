FROM ubuntu:20.04

ENV HOME="/"
ENV DEBIAN_FRONTEND=noninteractive
ENV PREFIX="$HOME/opt/cross"
ENV TARGET=i386-elf
ENV PATH="$PREFIX/bin:$PATH"

# Install required packages and build toolchain in a single layer
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    bison \
    flex \
    libgmp3-dev \
    libmpc-dev \
    libmpfr-dev \
    texinfo \
    libisl-dev \
    wget \
    make \
    ca-certificates \
    grub-pc-bin \
    xorriso \
    mtools \
    && mkdir -p /src \
    && cd /src \
    # Build binutils
    && wget -q https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.xz \
    && tar -xf binutils-2.44.tar.xz \
    && mkdir build-binutils \
    && cd build-binutils \
    && ../binutils-2.44/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror \
    && make -j$(nproc) \
    && make install \
    && cd /src \
    # Build GCC with only necessary components
    && wget -q https://ftp.gnu.org/gnu/gcc/gcc-14.2.0/gcc-14.2.0.tar.xz \
    && tar -xf gcc-14.2.0.tar.xz \
    && mkdir build-gcc \
    && cd build-gcc \
    && ../gcc-14.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers \
    && make -j$(nproc) all-gcc \
    && make -j$(nproc) all-target-libgcc \
    && make install-gcc \
    && make install-target-libgcc \
    # Clean up to reduce image size
    && cd / \
    && rm -rf /src \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /kernel

CMD ["/bin/bash"]
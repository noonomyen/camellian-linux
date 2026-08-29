FROM ubuntu:26.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    bc \
    bison \
    flex \
    libssl-dev \
    libelf-dev \
    libncurses-dev \
    python3 \
    python-is-python3 \
    git \
    cpio \
    lz4 \
    zstd \
    device-tree-compiler \
    kmod \
    ca-certificates \
    curl \
    tar \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /opt/clang-r383902 && \
    curl -sSL "https://android.googlesource.com/platform/prebuilts/clang/host/linux-x86/+archive/refs/tags/android-11.0.0_r48/clang-r383902.tar.gz" | tar -xz -C /opt/clang-r383902

RUN mkdir -p /opt/aarch64-linux-android-4.9 && \
    curl -sSL "https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/+archive/refs/tags/android-11.0.0_r48.tar.gz" | tar -xz -C /opt/aarch64-linux-android-4.9

RUN mkdir -p /opt/arm-linux-androideabi-4.9 && \
    curl -sSL "https://android.googlesource.com/platform/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.9/+archive/refs/tags/android-11.0.0_r48.tar.gz" | tar -xz -C /opt/arm-linux-androideabi-4.9

RUN mkdir -p /opt/ccache-4.14 && \
    curl -sSL "https://github.com/ccache/ccache/releases/download/v4.14/ccache-4.14-linux-x86_64-glibc.tar.xz" | tar -xJ --strip-components=1 -C /opt/ccache-4.14 && \
    ln -s /opt/ccache-4.14/ccache /bin/ccache

ENV PATH="/opt/clang-r383902/bin:/opt/aarch64-linux-android-4.9/bin:/opt/arm-linux-androideabi-4.9/bin:${PATH}"
ENV IS_CONTAINER=1

WORKDIR /workspace

CMD ["/bin/bash"]

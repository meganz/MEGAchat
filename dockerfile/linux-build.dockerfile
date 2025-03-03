# docker build --build-arg DISTRO=<DISTRO> -f /path/to/linux-build.dockerfile -t megachat-linux-build .
#
# Where <DISTRO> is one of the following:
# - ubuntu:22.04
# - ubuntu:24.04
#
# docker run -v /path/to/megachat:/mega/megachat megachat-linux-build

ARG DISTRO=ubuntu:22.04

FROM $DISTRO

ENV DEBCONF_NOWARNINGS=yes
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    autoconf \
    autoconf-archive \
    build-essential \
    cmake \
    curl \
    git \
    libasound2-dev \
    libglib2.0-dev \
    libpulse-dev \
    libx11-dev \
    libxcomposite-dev \
    libxdamage-dev \
    libxrandr-dev \
    libxtst-dev \
    nasm \
    pkg-config \
    python3 \
    qtbase5-dev \
    tar \
    unzip \
    zip

WORKDIR /mega

CMD ["sh", "-c", "\
        git clone https://github.com/microsoft/vcpkg.git && \
        cmake \
            -DVCPKG_ROOT=/mega/vcpkg \
            -DCMAKE_BUILD_TYPE=Debug \
            -S /mega/megachat \
            -B build && \
        cmake --build build -j $(nproc) \
    "]

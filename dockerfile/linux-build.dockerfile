# Dockerfile used to exercise the development environment of MEGAchat.
#
# How to build:
# docker build [--build-arg DISTRO=<DISTRO>] -f /path/to/linux-build.dockerfile -t megachat-linux-build .
#
# Where <DISTRO> is one of the following:
# - ubuntu:22.04
# - ubuntu:24.04
#
# How to run:
# docker run -v /path/to/MEGAchat:/mega/MEGAchat megachat-linux-build

ARG DISTRO=ubuntu:24.04

FROM $DISTRO

ARG DISTRO

WORKDIR /mega

RUN <<EOF

case "${DISTRO}" in
    "ubuntu:22.04"|"ubuntu:24.04")
        PACKAGES="\
            autoconf \
            autoconf-archive \
            automake \
            build-essential \
            cmake \
            curl \
            default-jdk \
            git \
            libasound2-dev \
            libglib2.0-dev \
            libpulse-dev \
            libtool \
            libx11-dev \
            libxcomposite-dev \
            libxdamage-dev \
            libxrandr-dev \
            libxtst-dev \
            make \
            nasm \
            openjdk-21-jdk \
            pkg-config \
            python3 \
            qtbase5-dev \
            swig \
            tar \
            unzip \
            zip \
            "
        ;;
    *)
        echo "Unsupported distro: $DISTRO"
        exit 2
        ;;
esac

export DEBCONF_NOWARNINGS=yes
export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get upgrade -y
apt-get install -y $PACKAGES

export JAVA_HOME="/usr/lib/jvm/java-21-openjdk-amd64"
export PATH="${JAVA_HOME}/bin:$PATH"

git clone https://github.com/microsoft/vcpkg.git

EOF

CMD ["sh", "-c", "\
        cmake \
            --preset dev-unix \
            -DCMAKE_BUILD_TYPE=Debug \
            -S MEGAchat && \
        cmake \
            --build build-MEGAchat-dev-unix \
            -j $(nproc) \
    "]

# Dockerfile for cross-compiling for Android and its different architectures.
#
# Build the Docker image:
#   docker build -t megachat-android-cross-build -f /path/to/your/MEGAchat/dockerfile/android-cross-build.dockerfile .
#     -t : Tags the built container with a name
#     -f : Specify dockerfile to be build, replace /path/to/your/MEGAchat with your local path to it
#
# Run the Docker container and build the project for a specific architecture:
#   docker run -v /path/to/your/MEGAchat:/mega/MEGAchat -v /path/to/your/vcpkg:/mega/vcpkg -e ARCH=[arm, arm64, x86, x64] [-e BUILD_SHARED_LIBS=ON] -it megachat-android-cross-build
#     -v : Mounts a local directory into the container, replace /path/to/your/MEGAchat and /path/to/your/vcpkg with your local paths
#     -e : Sets an environment variable, `ARCH` environment variable is used to specify the target architecture
#     -it : Starts an interactive terminal session inside the container after the cmake project is configured and build


# Manual test run for this file:
#
#    docker build -t megachat-android-cross-build -f android-cross-build.dockerfile .
#    docker run -v /c/_dev/mega/MEGAchat:/mega/MEGAchat -v /c/_dev/mega/vcpkg:/mega/vcpkg -it megachat-android-cross-build /bin/bash
#
#    #Build for arm64: export ARCH=arm64 && export VCPKG_TRIPLET='arm64-android-mega' && export ANDROID_ARCH='arm64-v8a'
#    #Build for arm:   export ARCH=arm && export VCPKG_TRIPLET='arm-android-mega' && export ANDROID_ARCH='armeabi-v7a'
#    #Build for x64:   export ARCH=x64 VCPKG_TRIPLET='x64-android-mega' ANDROID_ARCH='x86_64' DEFINE_BUILD_SHARED_LIBS_ON='BUILD_SHARED_LIBS=ON'
#    #Build for x86:   export ARCH=x86 && export VCPKG_TRIPLET='x86-android-mega' && export ANDROID_ARCH='x86'
#
#    cmake -B buildAndroid_${ARCH} -S MEGAchat -DVCPKG_ROOT=/mega/vcpkg -DCMAKE_BUILD_TYPE=RelWithDebInfo -DVCPKG_TARGET_TRIPLET=${VCPKG_TRIPLET} -DCMAKE_SYSTEM_NAME=Android -DCMAKE_ANDROID_ARCH_ABI=${ANDROID_ARCH} -DCMAKE_ANDROID_NDK=${ANDROID_NDK_HOME} -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON ${DEFINE_BUILD_SHARED_LIBS_ON}
#
#    cmake --build buildAndroid_${ARCH} -j10


# Base image
FROM ubuntu:24.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    autoconf \
    autoconf-archive \
    build-essential \
    cmake \
    curl \
    git \
    libasound2-dev \
    libglib2.0-dev \
    libgtk-3-dev \
    libpulse-dev \
    libtool-bin \
    nasm \
    openjdk-21-jdk \
    pkg-config \
    python3 \
    python3-pip \
    python3-pkg-resources \
    swig \
    unzip \
    wget \
    zip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /mega

# Download, extract and set the Android NDK
ARG MEGA_NDK_RELEASE=27b
ARG MEGA_NDK_ZIP=android-ndk-r${MEGA_NDK_RELEASE}-linux.zip
RUN mkdir -p /mega/android-ndk && \
    chmod 777 /mega && \
    cd /mega/android-ndk && \
    wget https://dl.google.com/android/repository/${MEGA_NDK_ZIP} && \
    unzip ${MEGA_NDK_ZIP} && \
    rm ${MEGA_NDK_ZIP}
ENV ANDROID_NDK_HOME=/mega/android-ndk/android-ndk-r${MEGA_NDK_RELEASE}
ENV PATH=$PATH:$ANDROID_NDK_HOME
ENV JAVA_HOME=$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64
ENV PATH=$PATH:$JAVA_HOME

# Set default architecture
ARG ARCH=x64

# Install AWS cli to use VCPKG cache in S4
RUN curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip" && \
    unzip awscliv2.zip && \
    ./aws/install

# Configure and build CMake command, this will be executed when running the container
CMD ["sh", "-c", "\
    userdel -f ubuntu && \
    owner_uid=$(stat -c '%u' /mega/MEGAchat) && \
    owner_gid=$(stat -c '%g' /mega/MEGAchat) && \
    groupadd -g $owner_gid me && \
    echo 'Adding \"me\" user...' && \
    useradd -r -M -u $owner_uid -g $owner_gid -d /mega -s /bin/bash me && \
    ( [ -d /mega/.cache ] && chown me:me /mega/.cache || :) && \
    case ${ARCH} in \
      arm) \
        export VCPKG_TRIPLET='arm-android-mega' && \
        export ANDROID_ARCH='armeabi-v7a';; \
      arm64) \
        export VCPKG_TRIPLET='arm64-android-mega' && \
        export ANDROID_ARCH='arm64-v8a';; \
      x86) \
        export VCPKG_TRIPLET='x86-android-mega' && \
        export ANDROID_ARCH='x86';; \
      x64) \
        export VCPKG_TRIPLET='x64-android-mega' && \
        export ANDROID_ARCH='x86_64';; \
      *) \
        echo 'Unsupported architecture: ${ARCH}' && exit 1;; \
    esac && \
    case ${BUILD_SHARED_LIBS} in \
      ON) \
        export DEFINE_BUILD_SHARED_LIBS_ON=-DBUILD_SHARED_LIBS=ON;; \
      OFF|'') \
        ;; \
      *) \
        echo 'error: Unsupported value for BUILD_SHARED_LIBS:' ${BUILD_SHARED_LIBS} && \
        echo 'Valid values are: ON | OFF' && \
        echo 'Build stopped.' && exit 1;; \
    esac && \
    su - me -w 'ANDROID_NDK_HOME,PATH,JAVA_HOME,VCPKG_TRIPLET,ANDROID_ARCH,DEFINE_BUILD_SHARED_LIBS_ON,AWS_ACCESS_KEY_ID,AWS_SECRET_ACCESS_KEY,AWS_ENDPOINT_URL,VCPKG_BINARY_SOURCES' -c ' \
    cmake \
        --preset mega-android \
        -S MEGAchat \
        ${DEFINE_BUILD_SHARED_LIBS_ON} \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DVCPKG_TARGET_TRIPLET=${VCPKG_TRIPLET} \
        -DCMAKE_ANDROID_ARCH_ABI=${ANDROID_ARCH} \
        -DCMAKE_ANDROID_NDK=${ANDROID_NDK_HOME} \
        -DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON && \
    cmake \
        --build build-MEGAchat-mega-android' && \
    exec /bin/bash"]

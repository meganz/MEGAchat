# ==== APP CONTAINER ===========================================================
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update -qq && apt-get upgrade -y
# SDK
RUN apt-get install -y build-essential curl zip unzip automake autoconf autoconf-archive nasm cmake git
# MEGAchat
RUN apt-get install -y python3-pkg-resources libglib2.0-dev libgtk-3-dev libasound2-dev libpulse-dev
# QtApp example / in case we don't want to build it cmake step should include ENABLE_CHATLIB_QTAPP=OFF
RUN apt-get install -y qtbase5-dev

# Dev & debugging
RUN apt-get install -y sudo apt-utils apt-file vim bash-completion emacs fish time

# Set en_US.UTF-8 encoding
RUN apt-get install --reinstall -y locales
RUN sed -i 's/# en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen
RUN locale-gen en_US.UTF-8
RUN update-locale LANG=en_US.UTF-8
RUN locale-gen en_US.UTF-8

# Install AWS cli to use VCPKG cache in S4
RUN curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip" && \
    unzip awscliv2.zip && \
    ./aws/install

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US
ENV LC_ALL=en_US.UTF-8

RUN mkdir -p /mega && \
    chmod 777 /mega

# Set up work directory
WORKDIR /mega

# Set default architecture
ARG ARCH=x64


# Set entrypoint
CMD ["sh", "-c", "\
    owner_uid=$(stat -c '%u' /mega/chat) && \
    owner_gid=$(stat -c '%g' /mega/chat) && \
    groupadd -g $owner_gid mega && \
    echo 'Adding \"mega\" user...' && \
    useradd -r -M -u $owner_uid -g $owner_gid -d /mega -s /bin/bash mega && \
    arch=${ARCH} && \
    su - mega -w 'PATH,ARCH,LANG,LANGUAGE,LC_ALL,AWS_ACCESS_KEY_ID,AWS_SECRET_ACCESS_KEY,AWS_ENDPOINT_URL,VCPKG_BINARY_SOURCES' -c ' \
    cmake -B build -S chat \
        -DVCPKG_ROOT=/mega/vcpkg \
        -DCMAKE_BUILD_TYPE=Debug \
        -DUSE_FREEIMAGE=OFF \
        -DUSE_FFMPEG=OFF \
        -DENABLE_CHATLIB_QTAPP=OFF && \
    cmake --build build' && \
    exec /bin/bash"]

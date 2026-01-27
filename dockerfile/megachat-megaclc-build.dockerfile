# Dockerfile to build megaclc binaries.
#
# How to build:
# docker build [--build-arg DISTRO=<DISTRO>] -f /path/to/megachat-ubuntu2204-build.dockerfile -t megachat-ubuntu-build .
#
# Where <DISTRO> is one of the following:
# - ubuntu:22.04
# - ubuntu:24.04
#
# How to run:
# docker run -v /path/to/MEGAchat:/mega/MEGAchat [-v /path/to/sdk:/mega/sdk] [-v /path/to/output:/mega/build-MEGAchat-megaclc-unix/] megachat-ubuntu-build

ARG DISTRO=ubuntu:22.04
FROM $DISTRO

ENV DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update -qq && apt-get upgrade -y
# SDK
RUN apt-get install -y build-essential libtool curl zip unzip automake autoconf autoconf-archive nasm cmake git
# MEGAchat
RUN apt-get install -y python3-pkg-resources libglib2.0-dev libgtk-3-dev libasound2-dev libpulse-dev

# Install AWS cli to use VCPKG cache in S4
RUN curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip" && \
    unzip awscliv2.zip && \
    ./aws/install

RUN mkdir -p /mega && \
    chmod 777 /mega

# Set up work directory
WORKDIR /mega

# Set entrypoint
CMD ["sh", "-c", "\
    owner_uid=$(stat -c '%u' /mega/MEGAchat) && \
    owner_gid=$(stat -c '%g' /mega/MEGAchat) && \
    u=$(getent passwd \"$owner_uid\" | cut -d: -f1) && if [ -n \"$u\" ] && [ \"$owner_uid\" != \"0\" ]; then userdel -f \"$u\"; fi  && \
    echo 'Adding \"mega\" group and user...' && \
    groupadd -g $owner_gid mega && \
    useradd -M -u $owner_uid -g $owner_gid -d /mega -s /bin/bash mega && \
    su - mega -w 'AWS_ACCESS_KEY_ID,AWS_SECRET_ACCESS_KEY,AWS_ENDPOINT_URL,VCPKG_BINARY_SOURCES' -c ' \
    cmake \
        --preset megaclc-unix \
        -S MEGAchat \
        $(test -d /mega/sdk && echo -DSDK_DIR=/mega/sdk) \
        -DCMAKE_BUILD_TYPE=Release && \
    cmake \
        --build build-MEGAchat-megaclc-unix' && \
    exec /bin/bash"]

# ==== APP CONTAINER ===========================================================
FROM ubuntu:20.04

ENV DEBIAN_FRONTEND=noninteractive

# Install required packages
RUN apt-get update -qq && apt-get upgrade -y
# SDK
RUN apt-get install -y build-essential curl zip unzip autoconf autoconf-archive nasm cmake git
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
ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US
ENV LC_ALL=en_US.UTF-8


# Add mega user matching host: replace 1000 uid and gid by relevant system user intended
ENV GID=1000
ENV UID=1000
## Create a group 'mega' with GID 1000
RUN groupadd -g $GID mega

## Create a user 'mega' with UID 1000 and assign it to the 'mega' group
RUN useradd -m -u $UID -g $GID -s /bin/bash mega

## Add the user to the sudoers file (if you want to give sudo permissions)
RUN echo 'mega ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

## Set the user 'mega' as the default user
USER mega

# Set the working directory for the mega user
WORKDIR /home/mega

# Set entrypoint
CMD ["bash"]

# This image is meant as a builder so remember to mount VCPKG_ROOT, CHAT-DIR, and BUILD-DIR to use host file system and leave the output after run. Assuming as the name of this image ubu-chat-qt:22.04
## Example 001 of builder use from host assuming same dir as required resources and bash as host's shell
##docker run -u $(id -u):$(id -g) -v $PWD/vcpkg:/home/mega/vcpkg -v $PWD/chat:/home/mega/chat -v $PWD/build:/home/mega/build ubu-chat-qt:22.04 cmake -DCMAKE_BUILD_TYPE=Debug -DVCPKG_ROOT=/home/mega/vcpkg -S /home/mega/chat -B /home/mega/build

## Example 002 without QtApp and qtbase dependency not installed
##docker run -u $(id -u):$(id -g) -v $PWD/vcpkg:/home/mega/vcpkg -v $PWD/chat:/home/mega/chat -v $PWD/build:/home/mega/build ubu-chat-qt:22.04 cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_CHATLIB_QTAPP=OFF -DVCPKG_ROOT=/home/mega/vcpkg -S /home/mega/chat -B /home/mega/build

## Building step is the same no matter the configuration
##docker run -u $(id -u):$(id -g) -v $PWD/vcpkg:/home/mega/vcpkg -v $PWD/chat:/home/mega/chat -v $PWD/build:/home/mega/build ubu-chat-qt:22.04 cmake --build /home/mega/build --parallel 14
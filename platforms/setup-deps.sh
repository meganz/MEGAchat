#!/bin/bash
set -e
if (( $# < 2 )); then
    echo "Usage: setup-deps.sh <platform> <buildroot-parent-dir> [static|shared]"
    exit 1
fi

owndir=`echo "$(cd "$(dirname "$0")"; pwd)"`
platform=$1
buildroot=$2

#determine whether we will build shared or static libs
if (( $# < 3 )) || [[ "$3" == "static" ]]; then
       shared=0
       configure_static_shared="--enable-static --disable-shared"
elif [[ "$3" == "shared" ]]; then
       shared=1
       configure_static_shared="--enable-shared --disable-static"
else
       echo "Unknown option for static/shared"
fi

echo -e "\
Dependency builder configuration:\n\
Pllatform        : \033[1;32m$platform\033[0;0m\n\
Install prefix   : $buildroot\n\
Build shared libs: $shared"
read -n1 -r -p "Press enter to continue if these settings are ok, or ctrl+c to abort..." key

if [ ! -d "$buildroot/usr" ]; then
    mkdir -p "$buildroot/usr"
fi
if [ ! -d "$buildroot/home" ]; then
    mkdir -p "$buildroot/home"
fi

function downloadAndUnpack
{
#url: $1, [filename: $2]
    cd "$buildroot/home"
    local url="$1"
    if (( $# < 2 )); then
        local file=${url##*/}
    else
        local file=$2
    fi
    if [[ -f "./$file.done" ]]; then
        echo "->Already downloaded: $file"
    else
        echo "->Downloading $file..."
        wget -O "./$file" -q --show-progress "$1"
        touch "./$file.done"
    fi

    if [[ "$file" =~ \.tar\.[^\.]+$ ]]; then
        local base=${file%.*.*}
        if [[ ! -d "./$base" ]]; then
            echo "Extracting tar archive '$file'..." 
            tar -xf "$file"
        fi
        cd "./$base"
    elif [[ "$file" =~ \.zip$ ]]; then
        local base=${file%%.*}
        if [ ! -d "./$base" ]; then
            mkdir "./$base"
            cd "./$base"
            echo "Extracting zip archive '$file'..."
            unzip "../$file"
        else
            cd "./$base"
        fi
    else
        echo "Dont know how to extract archive '$file'"
        exit 1
    fi
}
function cloneGitRepo
{
    local reponame="${1##*/}"
    local dir="${reponame%%.*}"
    if [ ! -d "$dir" ]; then
        echo "->Cloning git repo $1..."
        git clone $1
    else
        echo "->Repository $1 already cloned"
    fi
    cd "$dir"
}
  
cd "$buildroot/home"
function buildInstall
{
# $1 dependency tag, must match buildInstall_<tag>
# $2 url
# [$3 configure arguments]
# [$4 local download file name]

    cd "$buildroot/home"
    if [[ $2 =~ .*\.git$ ]]; then
        cloneGitRepo $2
    else
        downloadAndUnpack "$2" $4
    fi
    if [ -f "./.built-and-installed" ]; then
        echo "->Already installed $1"
    else
        echo "->Building $1..."
        if [[ `type -t "buildInstall_$1"` == "function" ]]; then
            eval "buildInstall_$1"
        else
            buildInstall_autotools "$3"
        fi
        touch "./.built-and-installed"
    fi
    cd ..
}

function buildInstall_openssl
{
# multiple builds cause path too large in the below file, so remove it
    rm -rf $buildroot/usr/man/man3/*
    if [[ "$platform" == "android" ]]; then
        ./Configure android-armv7
        make no-asm
    elif [[ "$platform" == "ios" ]]; then
        ./Configure iphoneos-cross --prefix="$buildroot/usr" --openssldir="$buildroot/usr"
# The config script does not add -arch armv7 to the top makefile, so we have to patch the makefile
        sed -i.bak -e's/^CFLAG=\(.*\)$/CFLAG=\1 -arch armv7/' ./Makefile
        sed -i.bak -e"s|\$(CROSS_TOP)/SDKs/\$(CROSS_SDK)|$IOSC_SYSROOT|" ./Makefile
        make -j10 depend all
        make -j10 install_sw
    else
        echo "Configuring openssl in a generic way"
        ./Configure
        make -j10
    fi
}

function buildInstall_autotools
{
# [$1 configure args]
    if [ ! -f ./configure ]; then
        if [ ! -f ./autogen.sh ]; then
            echo "No configure, nor autogen.sh script found, don't know how to configure autotools build"
            exit 1
        fi
        ./autogen.sh
    fi
    xconfigure $configure_static_shared "$1"
    make clean
    make -j10 install
}
function buildInstall_cryptopp
{
    cp -v "$owndir/cryptopp_CMakeLists.txt" ./CMakeLists.txt
    sed -i.bak -e"s/#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64/#if CRYPTOPP_BOOL_X86 || (CRYPTOPP_BOOL_X32 \&\& \\!defined(_arm) \&\& \\!defined(__arm__)) || CRYPTOPP_BOOL_X64/" ./cpu.h
    rm -rf ./build
    mkdir ./build
    cd ./build
    xcmake ..
    make -j10 install
    cd ..
}

function buildInstall_zlib
{
# specialized for arm
    if [ $shared == "0" ]; then
        local static="--static"
    fi
    ./configure --prefix="$buildroot/usr" $static --archs="-arch armv7"
    make -j10 install
}
   
buildInstall openssl   "https://www.openssl.org/source/openssl-1.0.2g.tar.gz"
buildInstall cares     "http://c-ares.haxx.se/download/c-ares-1.11.0.tar.gz"
buildInstall curl      "https://curl.haxx.se/download/curl-7.48.0.tar.bz2" "--disable-ftp --disable-gopher --disable-smtp --disable-imap --disable-pop --disable-smb --disable-manual --disable-tftt --disable-telnet --disable-dict --disable-rtsp --disable-ldap --disable-ldaps --disable-file --disable-sspi --disable-tls-srp --disable-ntlm-wb --disable-unix-sockets"
buildInstall cryptopp  "https://www.cryptopp.com/cryptopp563.zip"
buildInstall megasdk   "https://github.com/meganz/sdk.git" "--without-freeimage --without-sodium --enable-chat --disable-examples"

if [[ "$platform" == ios ]]; then
    buildInstall zlib      "http://zlib.net/zlib-1.2.8.tar.gz"
fi

cd $owndir/../third-party/libevent
buildInstall_autotools

#!/bin/bash
set -e
if (( $# < 2 )); then
    echo "Usage: setup-deps.sh <platform> <buildroot-parent-dir> [static|shared]"
    exit 1
fi

owndir=`echo "$(cd "$(dirname "$0")"; pwd)"`
platform=$1
buildroot=$2
if [[ "$platform" == "ios" ]] || [[ "$platform" == "android" ]]; then
    cpuarch=armv7
else
#FIXME: Maybe support x64 as well?
    cpuarch=i686
fi

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
#url: $1, [filename: $2], [$3 dir to unpack in]
    cd "$buildroot/home"
    local url="$1"
    if [[ ! -z "$2" ]]; then
	    local file="$2"
    else
        local file=${url##*/}
    fi
	
    if [[ -f "./$file.done" ]]; then
        echo "->Already downloaded: $file"
    else
        echo "->Downloading $file..."
        wget -O "./$file" -q --show-progress "$1"
        touch "./$file.done"
    fi
    if [[ ! -z "$3" ]]; then
	    local exdir="$3"
	    if [ -d "./$exdir" ]; then
		    return
		fi
		mkdir "./$exdir"
	    cd "./$exdir"
	fi
    if [[ "$file" =~ \.tar\.[^\.]+$ ]]; then
        local base=${file%.*.*}
		local cmd="tar -xf"
		local type="tar"
    elif [[ "$file" =~ \.zip$ ]]; then
        local base=${file%%.*}
	    local cmd="unzip"
		local type="zip"
    else
        echo "Dont know how to extract archive '$file'"
        exit 1
    fi
	if [[ ! -d "./$base" ]] || [[ ! -z "$exdir" ]]; then
		echo "Extracting $type archive '$file'..." 
        $cmd "./$file"
    fi
    if [[ -z "$exdir" ]]; then
		cd "./$base"
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
function fetchInstall
{
# $1 dependency tag, must match buildInstall_<tag>
# $2 url
# [$3 configure arguments]
# [$4 local download file name]
# [$5 dir to unpack]
    cd "$buildroot/home"
    if [[ $2 =~ .*\.git$ ]]; then
        cloneGitRepo $2
    else
        downloadAndUnpack "$2" $4 $5
    fi
	if [[ `type -t "buildInstall_$1"` == "function" ]]; then
		local func="$1"
	else
		local func="standard"
    fi
	callBuildInstall "$1" "$func" "$3"
}

function callBuildInstall
{
# $1 <package name>, $2 <func name>, [$3 configure flags]
    if [ -f "./.built-and-installed" ]; then
        echo "->Already installed $1"
    else
        echo "->Building $1..."
        eval "buildInstall_$2 '$3'" 
        touch "./.built-and-installed"
	fi
	cd ..

}

if [ "$platform" != "win" ]; then
function buildInstall_openssl
{
# multiple builds cause path too large in the below file, so remove it
    rm -rf $buildroot/usr/man/man3/*
    if [[ "$platform" == "android" ]]; then
        ./Configure android-armv7 no-asm --prefix="$buildroot/usr" --openssldir="$buildroot/usr"
        sed -i.bak -e's/-mandroid//' ./Makefile
        make depend all
        make install_sw
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
#autotools build
# [$1 configure args]
    if [ ! -f ./configure ]; then
        if [ ! -f ./autogen.sh ]; then
            echo "No configure, nor autogen.sh script found, don't know how to configure autotools build"
            exit 1
        fi
        ./autogen.sh
    fi
    xconfigure $configure_static_shared $1
    make clean
    make -j10 install
}

function bulndInstall_standard
{
    buildInstall_autotools $@
}

function buildInstall_cryptopp
{
    cp -v "$owndir/cryptopp_CMakeLists.txt" ./CMakeLists.txt
    sed -i.bak -e"s/#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64/#if CRYPTOPP_BOOL_X86 || (CRYPTOPP_BOOL_X32 \&\& \\!defined(_arm) \&\& \\!defined(__arm__)) || CRYPTOPP_BOOL_X64/" ./cpu.h
    callBuildInstall cryptopp cmake
}
function buildInstall_cmake
{
    rm -rf ./build
    mkdir ./build
    cd ./build
    xcmake $1 ..
    make -j10 install
    cd ..
}

function buildInstall_zlib
{
# specialized for arm
    if [ $shared == "0" ]; then
        local static="--static"
    fi
    ./configure --prefix="$buildroot/usr" $static --archs="-arch $cpuarch"
    make -j10 install
}
else
#Windows builds
#TODO Shared library builds are not fully supported yet, static mode must be used
wbroot=`cygpath -w "$buildroot/usr"`
if [[ "shared" == "1" ]]; then
    runtimeFlag="/MD"
else
    runtimeFlag="/MT"
fi
	
function buildInstall_openssl
{
  ./Configure VC-WIN32 no-asm --prefix="$buildroot/usr" --openssldir="$buildroot/usr"
  cmd /C ms\\do_ms.bat
  nmake -f ms\\nt.mak install
}
function buildInstall_curl
{
  sed -i.bak -e"s|#endif /\* HEADER_CURL_CONFIG_WIN32_H \*/|#define HTTP_ONLY 1\n#endif|" lib/config-win32.h
  cp -r "$owndir/win/curl_CMakeLists.txt" ./CMakeLists.txt
  callBuildInstall curl standard $1
}

function buildInstall_megasdk
{
  cp -r "$owndir/win/megasdk-vs2015" .
  msbuild.exe megasdk-vs2015/megasdk.vcxproj /t:Rebuild "/p:Configuration=Release;buildroot=$wbroot"
  cp -rv ./include/mega "$buildroot/usr/include"
  cp -v ./include/*.h "$buildroot/usr/include"
  cp -v ./megasdk-vs2015/Release/mega.lib "$buildroot/usr/lib"
  if [ -f ./megasdk-vs2015/Release/mega.pdb ]; then
      cp -vf ./megasdk-vs2015/Release/mega.pdb "$buildroot/usr/lib"
  fi
}

function buildInstall_zlib
{
	nmake -f win32\\Makefile.msc clean all
	cp -v ./zlib.lib ./zlib.pdb "$buildroot/usr/lib"
	cp -v ./zconf.h ./zlib.h "$buildroot/usr/include"
}

function buildInstall_cmake
{
# assume standard build on windows is a cmake build
# $1 cmake configure args
    rm -rf ./build
    mkdir -p ./build
	cd build
	cmake -G "NMake Makefiles"\
    	"-DCMAKE_PREFIX_PATH=$wbroot"\
	    "-DCMAKE_INSTALL_PREFIX=$wbroot"\
		"-DCMAKE_BUILD_TYPE=Release"\
		"-DCMAKE_C_FLAGS_RELEASE=$runtimeFlag /O2 /Ob2 /D NDEBUG"\
		"-DCMAKE_CXX_FLAGS_RELEASE=$runtimeFlag /O2 /Ob2 /D NDEBUG"\
		"-DoptBuildShared=$shared" $1 ..
	nmake install
	cd ..
}
function buildInstall_standard
{
  buildInstall_cmake $@
}

function buildInstall_cares
{
    cp "$owndir/win/cares_CMakeLists.txt" ./CMakeLists.txt
	callBuildInstall cares standard "$1"
}
function buildInstall_sqlite
{
	if [[ $shared == "1" ]]; then
	    cl sqlite3.c $runtimeFlag /O2 /Ob2 /D NDEBUG -link -dll -out:sqlite3.dll
		cp -v ./sqlite3.dll "$/buildroot/usr/lib"
	else
	    cl sqlite3.c -c $runtimeFlag /O2 /Ob2 /D NDEBUG 
	    lib sqlite3.obj -OUT:sqlite3.lib
	    cp -v ./sqlite3.lib "$buildroot/usr/lib"
	fi
	cp -v ./sqlite3.h ./sqlite3ext.h "$buildroot/usr/include"
}

function buildInstall_cryptopp
{
    cp -v "$owndir/cryptopp_CMakeLists.txt" ./CMakeLists.txt
    sed -i.bak -e"s/#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64/#if CRYPTOPP_BOOL_X86 || (CRYPTOPP_BOOL_X32 \&\& \\!defined(_arm) \&\& \\!defined(__arm__)) || CRYPTOPP_BOOL_X64/" ./cpu.h
    sed -i.bak -e"s/#if MASM_RDRAND_ASM_AVAILABLE/#if 0/" -e "s/#if MASM_RDSEED_ASM_AVAILABLE/#if 0/" ./rdrand.cpp
	callBuildInstall cryptopp standard "$1"
}
fi

#==============================================================
if [[ "$platform" != linux ]]; then   
    fetchInstall openssl   "https://www.openssl.org/source/openssl-1.0.2g.tar.gz"
    fetchInstall cares     "http://c-ares.haxx.se/download/c-ares-1.11.0.tar.gz"
    fetchInstall curl      "https://curl.haxx.se/download/curl-7.48.0.tar.bz2" "--disable-ftp --disable-gopher --disable-smtp --disable-imap --disable-pop --disable-smb --disable-manual --disable-tftt --disable-telnet --disable-dict --disable-rtsp --disable-ldap --disable-ldaps --disable-file --disable-sspi --disable-tls-srp --disable-ntlm-wb --disable-unix-sockets"
    fetchInstall cryptopp  "https://www.cryptopp.com/cryptopp563.zip" "" "" "" "cryptopp563"
    fetchInstall expat     "http://downloads.sourceforge.net/project/expat/expat/2.1.1/expat-2.1.1.tar.bz2?r=https%3A%2F%2Fsourceforge.net%2Fprojects%2Fexpat%2F&ts=1458829388&use_mirror=heanet" "" expat-2.1.1.tar.bz2
    fetchInstall sqlite    "https://www.sqlite.org/2016/sqlite-amalgamation-3120000.zip"
	# android NDK ships with zlib
    if [[ "$platform" != "android" ]]; then
        fetchInstall zlib      "http://zlib.net/zlib-1.2.8.tar.gz"
    fi
fi

fetchInstall megasdk   "https://github.com/meganz/sdk.git" "--without-freeimage --without-sodium --enable-chat --disable-examples"

cd $owndir/../third-party/libevent
if [ ! -f ./.built-and-installed ]; then
    callBuildInstall libevent cmake "-DEVENT__DISABLE_REGRESS=1 -DEVENT__DISABLE_TESTS=1 -DBUILD_TESTING=0"
    touch ./.built-and-installed
fi


#!/bin/bash
set -e
function printUsage
{
    echo -e \
"Usage: setup-deps.sh -p|--platform <macos|linux|android|ios|win>\n\
    --builddir <dir-containing /usr> (This is \033[1;31mNOT\033[0;0m the /usr sysroot,\n\
        but a dir containing it - because downloads are saved in builddir/home)\n\
    [-static] (Build all libraries static, with /MT on windows\n\
    [--qt] (Download and install qt - only for windows platform)\n\
    [-b|--batch] (Disable confirmation before start)\n\
    [-n|--nmake] (Windows only: Always use nmake instead of automatically\n\
        using the JOM drop-in replacement for nmake which supports parallel\n\
        builds, if found)"
    exit 1
}
if (( $# < 2 )); then
    printUsage
fi

owndir=`echo "$(cd "$(dirname "$0")"; pwd)"`
while [[ $# > 0 ]]
do
    key="$1"
    case $key in
    --builddir)
        if [[ $# < 2 ]]; then
           echo "No directory specified after --builddir"
           exit 1
        fi
        buildroot=$2
        shift
        ;;
    -p|--platform)
       if [[ $# < 2 ]]; then
          echo "No platform specified"
          exit 1
       fi
       platform=$2
       shift
       ;;
    -b|--batch)
        batch=1
        ;;
    -d|--debug)
        buildtype="Debug"
        ;;
    -static)
        shared=0
        ;;
    -qt)
        buildqt=1
        ;;
    -n|--nmake)
        nmake="nmake"
        ;;
    -h|--help)
        printUsage
        ;;
    *)
        echo "Unknown option '$1'"
        exit 1
        ;;
    esac
    shift # past argument or value
done

if [[ -z "$buildroot" ]]; then
    echo "No --builddir specified"
    exit 1
fi
if [[ -z "$platform" ]]; then
    echo "No --platform specified"
    exit 1
fi

if [[ "$platform" == "ios" ]] || [[ "$platform" == "android" ]]; then
    cpuarch=armv7
else
#FIXME: Maybe support x64 as well?
    cpuarch=i686
fi

#determine whether we will build shared or static libs
if [[ "$shared" == "0" ]]; then
       configure_static_shared="--enable-static --disable-shared"
else
       shared=1
       configure_static_shared="--enable-shared --disable-static"
fi
if [[ "$platform" != "win" ]]; then
    buildqt=0
    nmake=""
else
   if [[ "$buildqt" != "1" ]]; then
       buildqt=0
   fi
   if [[ "$nmake" != "nmake" ]]; then
    #usage of nmake not enforced, detect JOM and use it if found
       set +e
       which jom > /dev/null
       set -e
       if [[ "$?" == "0" ]]; then
           nmake="jom"
       else
           nmake="nmake"
       fi
    fi
fi

echo -e "\
Dependency builder configuration:\n\
Pllatform        : \033[1;32m$platform\033[0;0m\n\
Build directory  : $buildroot\n\
Build shared libs: $shared"

if [[ "$platform" == win ]]; then
echo -e "\
Build Qt:        : $buildqt\n\
NMake command    : $nmake"
fi

if [[ "$batch" != "1" ]]; then
    read -n1 -r -p "Press enter to continue if these settings are ok, or ctrl+c to abort..." key
fi

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

    if [[ -f "./$file.downloaded" ]]; then
        echo "->Already downloaded: $file"
    else
        echo "->Downloading $file..."
        rm -rf "./$file"
        wget -O "./$file" -q "$1"
        touch "./$file.downloaded"
    fi
    if [[ ! -z "$3" ]]; then
        # extract dir specified manually
        local exdir="$3"
        if [ -d "./$exdir" ]; then
            cd "./$exdir"
            return
        fi
        echo "Creating subdir '$exdir' to extract archive '$file'..."
        mkdir "./$exdir"
        cd "./$exdir"
        pathToArchive="../"
    else
        pathToArchive="./"
    fi
    if [[ "$file" =~ \.tar\.[^\.]+$ ]]; then
        local base=${file%.*.*}
        local cmd="tar -xf"
        local type="tar"
    elif [[ "$file" =~ \.zip$ ]]; then
        local base=${file%.*}
        local cmd="unzip"
        local type="zip"
    else
        echo "Dont know how to extract archive '$file'"
        exit 1
    fi
    #if exdir was set and exdir existed, we would have bailed out earlier
    if ( [[ -z "$exdir" ]] && [ ! -d "./$base" ] ) || [[ ! -z "$exdir" ]]; then
        echo "Extracting $type archive '$file'..."
        $cmd "$pathToArchive$file"
    fi
    if [[ -z "$exdir" ]]; then
        cd "./$base"
    fi
}
function cloneGitRepo
{
    local reponame="${1##*/}"
    local dir="${reponame%%.*}"
    if [ ! -f "./$dir.downloaded" ]; then
        echo "->Cloning git repo $1..."
        rm -rf "./$dir"
        git clone $1
        touch "./$dir.downloaded"
    else
        echo "->Repository $1 already cloned"
    fi
    cd "$dir"
    if [[ ! -z "$2" ]]; then
        echo "Switching git repo '$1' to branch $2..."
        git checkout "$2"
    fi
}

cd "$buildroot/home"
function fetchInstall
{
# $1 dependency tag, must match buildInstall_<tag>
# $2 url
# [$3 configure arguments]
# [$4 local download file name / git branch name]
# [$5 dir to unpack]
    cd "$buildroot/home"
    if [[ $2 =~ .*\.git$ ]]; then
        cloneGitRepo $2 $4
    else
        downloadAndUnpack "$2" "$4" "$5"
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
        make -j4 depend all
        make -j4 install_sw
    else
        echo "Configuring openssl in a generic way"
        ./Configure
        make -j4
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
    make -j4 install
}

function buildInstall_standard
{
    buildInstall_autotools "$@"
}

function buildInstall_cryptopp
{
    cp -v "$owndir/cryptopp_CMakeLists.txt" ./CMakeLists.txt
    sed -i.bak -e"s/#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64/#if CRYPTOPP_BOOL_X86 || (CRYPTOPP_BOOL_X32 \&\& \\!defined(_arm) \&\& \\!defined(__arm__)) || CRYPTOPP_BOOL_X64/" ./cpu.h
    buildInstall_cmake "$@"
}

function buildInstall_cmake
{
    rm -rf ./build
    mkdir ./build
    cd ./build
    xcmake $1 ..
    make -j4 install
    cd ..
}

function buildInstall_zlib
{
# specialized for arm
    if [ $shared == "0" ]; then
        local static="--static"
    fi
    ./configure --prefix="$buildroot/usr" $static --archs="-arch $cpuarch"
    make -j4 install
}

function buildInstall_sqlite
{
    if [[ $shared == "1" ]]; then
        $CC $CPPFLAGS $CFLAGS sqlite3.c -fPIC -DSQLITE_API= -O2 -shared -D NDEBUG -o ./libsqlite3.so
        chmod a+x ./libsqlite3.so
        cp -v ./libsqlite3.so "$buildroot/usr/lib"
    else
        $CC $CPPFLAGS $CFLAGS sqlite3.c -c -O2 -D NDEBUG -o ./sqlite3.o
        ar -rcs ./libsqlite3.a ./sqlite3.o
        cp -v ./libsqlite3.a "$buildroot/usr/lib"
    fi
    cp -v ./sqlite3.h ./sqlite3ext.h "$buildroot/usr/include"
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
  if [[ "$shared" == 1 ]]; then
      $nmake -f ms\\ntdll.mak install
  else
      $nmake -f ms\\nt.mak install
  fi
}
function buildInstall_curl
{
  sed -i.bak -e"s|#endif /\* HEADER_CURL_CONFIG_WIN32_H \*/|#define HTTP_ONLY 1\n#endif|" lib/config-win32.h
  cp -v "$owndir/win/curl_CMakeLists.txt" ./CMakeLists.txt
  buildInstall_cmake "$@"
}

function buildInstall_megasdk
{
  cp -v "$owndir/win/megasdk_CMakeLists.txt" ./CMakeLists.txt
  cp -v "$owndir/win/megasdk_config.h.in" ./config.h.in
  buildInstall_cmake "$@"
  #cp -rv ./include/mega "$buildroot/usr/include"
  #cp -v ./include/*.h "$buildroot/usr/include"
  #cp -v ./megasdk-vs2015/Release/mega.lib "$buildroot/usr/lib"
  #if [ -f ./megasdk-vs2015/Release/mega.pdb ]; then
  #    cp -vf ./megasdk-vs2015/Release/mega.pdb "$buildroot/usr/lib"
  #fi
}

function buildInstall_zlib
{
    $nmake -f win32\\Makefile.msc clean all
    rm -fv "$buildroot/usr/lib/libz.*"
    rm -fv "$buildroot/usr/lib/zlib*.*"

    if [[ "$shared" == 1 ]]; then
        cp -v ./zlib1.dll "$buildroot/usr/lib"
        cp -v ./zdll.lib "$buildroot/usr/lib/libz.lib"
    else
        cp -v ./zlib.lib "$buildroot/usr/lib/libz.lib"
        cp -v ./zlib.pdb "$buildroot/usr/lib/libz.pdb"
    fi
    cp -v ./zconf.h ./zlib.h "$buildroot/usr/include"
}
function buildInstall_libsodium
{
    cp -v "$owndir/libsodium_CMakeLists.txt" ./CMakeLists.txt
    buildInstall_cmake "$@"
}

function buildInstall_cmake
{
# $1 cmake configure args
    rm -rf ./build
    mkdir -p ./build
    cd build
    generator="NMake Makefiles"
    if [[ "$nmake" == "jom" ]]; then
        generator="$generator JOM"
    fi
    cmake -G "$generator"\
        "-DCMAKE_PREFIX_PATH=$wbroot"\
        "-DCMAKE_INSTALL_PREFIX=$wbroot"\
        "-DCMAKE_BUILD_TYPE=Release"\
        "-DCMAKE_C_FLAGS_RELEASE=$runtimeFlag /O2 /Ob2 /D NDEBUG"\
        "-DCMAKE_CXX_FLAGS_RELEASE=$runtimeFlag /O2 /Ob2 /D NDEBUG"\
        "-DoptBuildShared=$shared" $1 ..
    $nmake install
    cd ..
}
function buildInstall_standard
{
# assume standard build on windows is a cmake build
  buildInstall_cmake "$@"
}

function buildInstall_cares
{
    cp "$owndir/win/cares_CMakeLists.txt" ./CMakeLists.txt
    buildInstall_cmake "$@"
}
function buildInstall_sqlite
{
    if [[ $shared == "1" ]]; then
        cl sqlite3.c $runtimeFlag "-DSQLITE_API=__declspec(dllexport)" /O2 /Ob2 /D NDEBUG -link -dll -out:sqlite3.dll
        cp -v ./sqlite3.dll "$buildroot/usr/lib"
    else
        cl sqlite3.c -c $runtimeFlag /O2 /Ob2 /D NDEBUG
        lib sqlite3.obj -OUT:sqlite3.lib
    fi
    cp -v ./sqlite3.lib "$buildroot/usr/lib"
    cp -v ./sqlite3.h ./sqlite3ext.h "$buildroot/usr/include"
}

function buildInstall_cryptopp
{
    cp -v "$owndir/cryptopp_CMakeLists.txt" ./CMakeLists.txt
    sed -i.bak -e"s/#if CRYPTOPP_BOOL_X86 || CRYPTOPP_BOOL_X32 || CRYPTOPP_BOOL_X64/#if CRYPTOPP_BOOL_X86 || (CRYPTOPP_BOOL_X32 \&\& \\!defined(_arm) \&\& \\!defined(__arm__)) || CRYPTOPP_BOOL_X64/" ./cpu.h
    sed -i.bak -e"s/#if MASM_RDRAND_ASM_AVAILABLE/#if 0/" -e "s/#if MASM_RDSEED_ASM_AVAILABLE/#if 0/" ./rdrand.cpp
    buildInstall_cmake "$@"
}
function buildInstall_qt
{
    if [[ "$shared" == "1" ]]; then
       local sflag="-shared"
    else
       local sdlag="-static -static-runtime"
    fi
    cmd /C configure.bat $sflag -release  -no-ssl -opensource -confirm-license\
        -nomake examples -nomake tests -prefix "$wbroot" -skip\
        qtdeclarative -skip qtwebengine -skip qtlocation -skip qtsensors -skip qtmultimedia\
        -skip qtconnectivity -skip qtwebsockets -skip qtwebchannel -skip qtserialport\
        -skip qtserialbus -skip qttools -skip qtscript -skip qtwayland -skip qtactiveqt
    $nmake install
}
fi

#==============================================================
if [[ "$platform" != linux ]]; then
    fetchInstall openssl   "https://www.openssl.org/source/openssl-1.0.2g.tar.gz"
    fetchInstall cares     "http://c-ares.haxx.se/download/c-ares-1.11.0.tar.gz"
    fetchInstall curl      "https://curl.haxx.se/download/curl-7.48.0.tar.bz2" "--disable-ftp --disable-gopher --disable-smtp --disable-imap --disable-pop --disable-smb --disable-manual --disable-tftt --disable-telnet --disable-dict --disable-rtsp --disable-ldap --disable-ldaps --disable-file --disable-sspi --disable-tls-srp --disable-ntlm-wb --disable-unix-sockets"
    fetchInstall cryptopp  "https://www.cryptopp.com/cryptopp563.zip" "" "" "cryptopp563"
    fetchInstall libsodium "https://download.libsodium.org/libsodium/releases/libsodium-1.0.10.tar.gz"
    fetchInstall expat     "http://downloads.sourceforge.net/project/expat/expat/2.1.1/expat-2.1.1.tar.bz2?r=https%3A%2F%2Fsourceforge.net%2Fprojects%2Fexpat%2F&ts=1458829388&use_mirror=heanet" "" expat-2.1.1.tar.bz2
#   fetchInstall readline  "http://git.savannah.gnu.org/cgit/readline.git/snapshot/readline-master.tar.gz"
    fetchInstall sqlite    "https://www.sqlite.org/2016/sqlite-amalgamation-3130000.zip"
    # android NDK ships with zlib
    if [[ "$platform" != "android" ]]; then
        fetchInstall zlib      "http://zlib.net/zlib-1.2.8.tar.gz"
    fi
fi

if [[ -z "$MEGASDK_BRANCH" ]]; then
    MEGASDK_BRANCH=develop
fi

fetchInstall megasdk "https://github.com/meganz/sdk.git" "--enable-chat --without-freeimage --disable-sync --disable-examples --disable-tests" $MEGASDK_BRANCH

if [[ "$buildqt" == '1' ]]; then
  fetchInstall qt "http://download.qt.io/official_releases/qt/5.6/5.6.0/single/qt-everywhere-opensource-src-5.6.0.zip"
fi

echo "============== All done ==================="

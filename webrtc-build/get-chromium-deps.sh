#!/bin/bash
#This script must be run in the src directory of the webrtc checkout,
#i.e. the chromium directory will be created inside the working dir

if (( $# < 1 )); then
    echo "Usage: `basename $0` <revision|branch>"
    exit 1
fi

revision="$1"
oriwd=`pwd`
karere=$(dirname $0)

set -e

function getfile
{
#$1 url, $2 filename
    if [ -f "$2" ]; then
        echo "Deleting existing file $2"
        rm -f "$2"
    fi
    echo "Downloading file $2...($1)"
    wget -O - -o /dev/null "$1" | base64 --decode > "$2"
}

function getdir
{
#$1 url, $2 dirname
   if [ -d "$2" ] && [ -f "$2/.syncdone" ]; then
       echo "$2 already downloaded, skipping"
       return 0
   fi
   mkdir -p "$2"
   echo "Downloading directory $2 ($1)..."
   wget -O - -o /dev/null "$1" | tar -xz -C "$2"
   touch "$2/.syncdone"
}

function getDirsAndFiles
{
    #$1 path, $2 filelist, $3 dirlist
    if [ ! -z "$1" ]; then
        mkdir -p "$1"
        path="$1/"
    else
        path=""
    fi

    for file in $2
    do
        if [ ${file:0:1} == "#" ]; then
            continue
        fi
        getfile "https://chromium.googlesource.com/chromium/src.git/+/$revision/$path$file?format=TEXT" "./$path$file"
    done

    for dir in $3
    do
        if [ ${dir:0:1} == "#" ]; then
            continue
        fi
        getdir "https://chromium.googlesource.com/chromium/src.git/+archive/$revision/$path$dir.tar.gz" "./$path$dir"
    done
}
function getPlatform
{
    local name=`uname`
    if [[ "$name" == "Darwin" ]]; then
        platform="macos"
    elif [[ "$name" == "Linux" ]]; then
        platform="linux"
    else
       echo "Unsupported platform"
       exit 2
    fi
}
if [ ! -d "src" ] && [ -f "../.get-chromium-deps-ran" ]; then
    rm -f "../.get-chromium-deps-ran"
    echo "Chromium deps flagged as fetched, but chromium/src dir does not exist, removing flag"
fi
if [ -f "../.get-chromium-deps-ran" ]; then
    echo 'Chromium files already downloaded and linked. Skipping'
    exit 0
fi

oriwd=`pwd`
cd chromium
mkdir -p './src'
cd 'src'

getPlatform
files="BUILD.gn DEPS"
dirs="build tools buildtools testing"
files3p="$files"
dirs3p=`cat $karere/third-party-deps.txt`
if [ -f "$karere/$platform/third-party-deps.txt" ]; then
    dirs3p="$dirs3p `cat $karere/$platform/third-party-deps.txt`"
fi

getDirsAndFiles "" "$files" "$dirs"
getDirsAndFiles "third_party" "$files3p" "$dirs3p"

echo "Removing dependencies and hooks from original chromium DEPS file..."
python $karere/remove-from-DEPS.py ./DEPS $karere/chromium-deps-del.txt

#cd from chromium/src to chromium
cd ".."
rm -f ./.gclient
gclient config --name src --unmanaged "https://test"
echo "==========================================================================="
echo "Selectively downloading repos of Chromium deps"
echo "via gclient sync --force on the stripped DEPS file..."
gclient sync --force --no-history
echo "Done"

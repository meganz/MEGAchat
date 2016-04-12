#!/bin/bash
#This script must be run in the src directory of the webrtc checkout,
#i.e. the chromium directory will be created inside the working dir

if (( $# < 2 )); then
    echo "Usage: `basename $0` <platform> <revision|branch>"
    echo "platform is one of: linux, mac, android, ios"
    exit 1
fi

platform="$1"
revision="$2"
oriwd=`pwd`
karere=$(dirname $0)

set -e

#implements getFile and getDir
source $karere/git-partial-download.sh

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
        getFile "https://chromium.googlesource.com/chromium/src.git/+/$revision/$path$file" "./$path$file"
    done

    for dir in $3
    do
        if [ ${dir:0:1} == "#" ]; then
            continue
        fi
        getDir "https://chromium.googlesource.com/chromium/src.git/+archive/$revision/$path$dir.tar.gz" "./$path$dir"
    done
}

if [ ! -d "src" ] && [ -f "../.get-chromium-deps-ran" ]; then
    rm -f "../.get-chromium-deps-ran"
    echo "Chromium deps flagged as fetched, but chromium/src dir does not exist, removing flag"
fi
if [ -f "../.get-chromium-deps-ran" ]; then
    echo 'Chromium files already downloaded and linked. Skipping'
    exit 0
fi

if [ -z "$platform" ]; then
    echo "'platform' env variable not set, you should not run this script directly"
    exit 1
fi

oriwd=`pwd`
cd chromium
mkdir -p './src'
cd 'src'

files="BUILD.gn DEPS"
dirs="build tools"
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
if [ ! -z "$gclientConfigOpts" ]; then
    echo "$gclientConfigOpts" >> ./.gclient
fi

echo "==========================================================================="
echo "Selectively downloading repos of Chromium deps"
echo "via gclient sync --force on the stripped DEPS file..."
#gclient sync --force --no-history
echo "Done"

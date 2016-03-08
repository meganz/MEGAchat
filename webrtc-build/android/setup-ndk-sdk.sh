#!/bin/bash
rev=$1
if [ -z "$rev" ]; then
    rev="master"
fi

karere=`echo "$(cd "$(dirname "$0")"/..; pwd)"`
echo "karere=$karere"
source $karere/git-partial-download.sh
aturl="https://chromium.googlesource.com/android_tools.git"
atdir="chromium/src/third_party/android_tools"
fileurl="$aturl/+/$rev"
dirurl="$aturl/+archive/$rev"
support="sdk/extras/android/support"
v7="$support/v7"

function getDirByPath
{
    getDir "$dirurl/${1}.tar.gz" "$atdir/$1"
}
function getFileByPath
{
    local file="$atdir/${1}"
    if [ -f "$file.syncdone" ]; then
        echo "->Skipping file '$file', already downloaded"
        return
    fi
    getFile "$fileurl/${1}" "$file"
    touch "$file.syncdone"
}

getFile "$atdir/android_tools.gyp" "chromium/src/tools/android/android_tools.gyp"
getFileByPath "sdk/platforms/android-23/android.jar"
getDirByPath "sdk/build-tools/23.0.1"

# These are some of the requirements to build the android test app
#getDirByPath "$v7/appcompat/res"
#getDirByPath "$v7/mediarouter/res"
#getDirByPath "$v7/recyclerview/res"
#getDirByPath "$support/design/res"
#getDirByPath "sdk/tools/lib"

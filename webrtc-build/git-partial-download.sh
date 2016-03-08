#!/bin/bash

function getFile
{
#$1 url, $2 filename
    if [ -f "$2" ]; then
        echo "Deleting existing file $2"
        rm -f "$2"
    fi
    local filedir=`dirname "$2"`
    if [ ! -d "$filedir" ]; then
        echo "getFile: Creating dir $filedir"
        mkdir -p "$filedir"
    fi 
    echo "-> Downloading file $2...($1)"
    wget -O - -o /dev/null "$1?format=TEXT" | base64 --decode > "$2"
}

function getDir
{
#$1 url, $2 parentDir
   if [ -d "$2" ] && [ -f "$2/.syncdone" ]; then
       echo "-> Skipping dir $2, already downloaded"
       return 0
   fi
   if [ ! -d "$2" ]; then
       echo "getDir: Creating dir $2"
       mkdir -p "$2"
   fi
   echo "-> Downloading directory $2 ($1)..."
   wget -O - -o /dev/null "$1" | tar -xzf - -C "$2"
   touch "$2/.syncdone"
}

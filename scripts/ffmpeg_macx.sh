#!/bin/bash

function build_ffmpeg
{
   PREFIX=$PWD/ffmpeg_macx

   read -rsp $'Select type ffmpeg (1-git, 2-archive):\n' -n1 key
   
   #delete if exist
   if [ -d $PREFIX ]; then
      rm -rf $PREFIX
   fi

   #delete if exist
   if [ -d ffmpeg ]; then
      rm -rf ffmpeg
   fi

   if [ $key == 1 ]; then
     git clone git://source.ffmpeg.org/ffmpeg.git ffmpeg
   elif [ $key == 2 ]; then
     FILE="ffmpeg-snapshot.tar.bz2"
     if [ -f "$FILE" ]; then
        tar xjvf $FILE
     else
        curl http://ffmpeg.org/releases/$FILE -o $FILE
        tar xjvf $FILE
     fi
   else
     echo "!!!Source type is not selected, exit..."
     exit 1
   fi

   cd ffmpeg

   ./configure \
    --prefix=$PREFIX \
    --enable-version3 \
    --arch=x86_64 \
    --cc='cc -m64' \
    --enable-shared \
    --disable-static \
    --disable-doc \
    --disable-ffmpeg \
    --disable-ffplay \
    --disable-ffprobe \
    --disable-ffserver \
    --disable-avdevice \
    --disable-symver \
    --disable-encoders \
    --disable-muxers \
    --disable-yasm
   
   make clean
   make -j4
   sudo make install
}

build_ffmpeg



#!/bin/bash

NDK=$HOME/Library/Android/android-ndk-r10e
SYSROOT=$NDK/platforms/android-9/arch-arm/
TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.8/prebuilt/darwin-x86_64

function build_ffmpeg
{
   CPU=arm
   PREFIX=$PWD/ffmpeg_android
   ADDI_CFLAGS="-marm"

   read -rsp $'Select type ffmpeg (1-git, 2-archive):\n' -n1 key
   
   delete if exist
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
        wget http://ffmpeg.org/releases/$FILE
        tar xjvf $FILE
     fi
   else
     echo "!!!Source type is not selected, exit..."
     exit 1
   fi

   cd ffmpeg

   #------------------
   #change output format
   #------------------
   #change
   #SLIBNAME_WITH_MAJOR='$(SLIBNAME).$(LIBMAJOR)'
   #LIB_INSTALL_EXTRA_CMD='$$(RANLIB) "$(LIBDIR)/$(LIBNAME)"'
   #SLIB_INSTALL_NAME='$(SLIBNAME_WITH_VERSION)'
   #SLIB_INSTALL_LINKS='$(SLIBNAME_WITH_MAJOR) $(SLIBNAME)'
   #to
   #SLIBNAME_WITH_MAJOR='$(SLIBPREF)$(FULLNAME)-$(LIBMAJOR)$(SLIBSUF)'
   #LIB_INSTALL_EXTRA_CMD='$$(RANLIB) "$(LIBDIR)/$(LIBNAME)"'
   #SLIB_INSTALL_NAME='$(SLIBNAME_WITH_MAJOR)'
   #SLIB_INSTALL_LINKS='$(SLIBNAME)' 
   
   strMajorOld="SLIBNAME_WITH_MAJOR='\$(SLIBNAME).\$(LIBMAJOR)'"
   strMajorNew="SLIBNAME_WITH_MAJOR='\$(SLIBPREF)\$(FULLNAME)-\$(LIBMAJOR)\$(SLIBSUF)'"

   strNameOld="SLIB_INSTALL_NAME='\$(SLIBNAME_WITH_VERSION)'"
   strNameNew="SLIB_INSTALL_NAME='\$(SLIBNAME_WITH_MAJOR)'"

   strLinksOld="SLIB_INSTALL_LINKS='\$(SLIBNAME_WITH_MAJOR)[[:space:]]\$(SLIBNAME)'"
   strLinksNew="SLIB_INSTALL_LINKS='\$(SLIBNAME)'"

   sed -i.bak s/$strMajorOld/$strMajorNew/g ./configure
   sed -i.bak s/$strNameOld/$strNameNew/g ./configure
   sed -i.bak s/$strLinksOld/$strLinksNew/g ./configure

   ./configure \
    --prefix=$PREFIX \
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
    --cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- \
    --target-os=linux \
    --arch=arm \
    --enable-cross-compile \
    --sysroot=$SYSROOT \
    --extra-cflags="-Os -fpic $ADDI_CFLAGS" \
    --extra-ldflags="$ADDI_LDFLAGS" \
    $ADDITIONAL_CONFIGURE_FLAG
   
   make clean
   make -j2
   make install
}

build_ffmpeg



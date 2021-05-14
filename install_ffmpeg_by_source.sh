#!/bin/bash

yum install -y gcc gcc-c++ autoconf automake bzip2 bzip2-devel cmake freetype-devel git libtool make pkgconfig zlib-devel

export SRC=/usr/local
export SRC_BIN=$SRC/bin
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig
export MAKEFLAGS="-j$(($(nproc) + 1))"
export LD_LIBRARY_PATH=${SRC}/lib
echo "${SRC}/lib" | sudo tee /etc/ld.so.conf.d/ffmpeg.conf

export build_dir=$HOME/ffmpeg_build
mkdir -p ${build_dir}
echo "build_dir: ${build_dir}"

########################################################
## nasm
########################################################
cd ~/ffmpeg_sources
curl -O -L https://www.nasm.us/pub/nasm/releasebuilds/2.15.05/nasm-2.15.05.tar.bz2
rm -rf nasm-2.15.05
tar xjvf nasm-2.15.05.tar.bz2
cd nasm-2.15.05
./autogen.sh
./configure --prefix="$SRC" --bindir="$SRC_BIN"
make
make install
rm -rf nasm-2.15.05

########################################################
## yasm
########################################################
cd ~/ffmpeg_sources
curl -O -L https://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz
rm -rf yasm-1.3.0
tar xzvf yasm-1.3.0.tar.gz
cd yasm-1.3.0
./configure --prefix="$SRC" --bindir="$SRC_BIN"
make
make install
rm -rf yasm-1.3.0


########################################################
## x264
########################################################
cd ~/ffmpeg_sources
git clone --branch stable --depth 1 https://code.videolan.org/videolan/x264.git
rm -rf x264
tar xzvf x264.tar.gz
cd x264
./configure --prefix="$SRC" --bindir="$SRC_BIN" --enable-pic --enable-shared --disable-cli
make
make install
rm -rf x264

########################################################
## x265
########################################################
cd ~/ffmpeg_sources
git clone --branch stable --depth 2 https://bitbucket.org/multicoreware/x265_git
rm -rf x265_git
tar xzvf x265_git.tar.gz
cd x265_git/build/linux
cmake -G "Unix Makefiles" -DCMAKE_INSTALL_PREFIX="$SRC" -DENABLE_SHARED:bool=on ../../source
make
make install
rm -rf x265_git


cd ~/ffmpeg_sources
git clone --depth 1 https://github.com/mstorsjo/fdk-aac
rm -rf fdk-aac
tar xzvf fdk-aac.tar.gz
cd fdk-aac
autoreconf -fiv
./configure --prefix="$SRC"
make
make install
rm -rf fdk-aac


cd ~/ffmpeg_sources
curl -O -L https://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz
rm -rf lame-3.100
tar xzvf lame-3.100.tar.gz
cd lame-3.100
./configure --prefix="$SRC" --bindir="$SRC_BIN" --enable-nasm
make
make install
rm -rf lame-3.100

cd ~/ffmpeg_sources
curl -O -L https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz
rm -rf opus-1.3.1
tar xzvf opus-1.3.1.tar.gz
cd opus-1.3.1
./configure --prefix="$SRC"
make
make install
rm -rf opus-1.3.1

cd ~/ffmpeg_sources
git clone --depth 1 https://chromium.googlesource.com/webm/libvpx.git
rm -rf libvpx
tar xzvf libvpx.tar.gz
cd libvpx
./configure --prefix="$SRC" --disable-examples --disable-unit-tests --enable-vp9-highbitdepth --as=yasm
make
make install
rm -rf libvpx

cd ~/ffmpeg_sources
curl -O -L https://ffmpeg.org/releases/ffmpeg-snapshot.tar.bz2
rm -rf ffmpeg-4.4
tar xjvf ffmpeg-4.4.tar.bz2
cd ffmpeg-4.4
./configure \
  --prefix="$SRC" \
  --extra-cflags="-I$SRC/include" \
  --extra-ldflags="-L$SRC/lib" \
  --extra-libs=-lpthread \
  --extra-libs=-lm \
  --bindir="$SRC_BIN" \
  --enable-gpl \
  --enable-libfdk_aac \
  --enable-libfreetype \
  --enable-libmp3lame \
  --enable-libopus \
  --enable-libvpx \
  --enable-libx264 \
  --enable-libx265 \
  --enable-nonfree
make
make install
hash -d ffmpeg
rm -rf ffmpeg-4.4

ldconfig # update ld.so.cache
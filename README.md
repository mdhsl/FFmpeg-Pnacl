# FFMPEG Pnacl Port

This is a webport of the FFMPeg library into Pnacl. It's inspired from http://neo.idletime.tokyo/enu/ffmpeg_pnacl/. 

# Features

- FFmpeg pexe file
- LibAv libraries (decode/encode)
- Support of H264, (Optional: H265, VPX)

# Aim

This port has been made to decode encoded H264 video frames in real-time within/for the OSH Toolkit (https://github.com/opensensorhub/osh-js). The OSH-js toolkit already a ported version of FFMpeg using emscripten but for FULL HD stream (and more) it was too slow. This project is intended to get real-time video with low lantecy using hardware acceleration within your web browser.


# Requirements
  - Clone this git directory
  - Install and setup the Pnacl Toolkit (https://developer.chrome.com/native-client/sdk/download)

# Installation

Export your PNacl SDK env variable

```sh
$ export NACL_SDK_ROOT=<SDK_PATH>/nacl_sdk/pepper_56
$ export PATH=$NACL_SDK_ROOT/toolchain/linux_pnacl/bin:$PATH
```

## Build x264
Go to the "pnacl-ffmpeg-3.0.1" directory
```sh
$ cd pnacl-ffmpeg-3.0.1/x264-snapshot-20160103-2245-stable/
$ ./naclconfig
$ make
$ make install
```

The config file has been setup to install directly into your NACL_SDK_ROOT directory

## Build FFMPeg

```sh
$ cd ..
$ cd ffmpeg-3.0.1/
$ ./naclconfig
$ make
$ make install
```

The config file has been setup to install directly into your NACL_SDK_ROOT directory

Now you have install the x264, ffmpeg and libAv libraries into your Pnacl SDK. You can use your SDK by including these libraries.

# Example

The OSH example connects to a local OSH server to get a H264 stream and decode it using libav libs.

Go to the example:
```sh
$ cd ../..
$ cd cd pnacl-ffmpeg-example/
```

At this point, you have a first Makefile allowing you to serve a local server by using:
```sh
$ make serve
```
It will provide you a http local url to test the App.

If you want to modify the C++ code, you have to go to the osh integration directory:

```sh
$ cd osh-integration-example/
$ make
```
It will generate a new pexe file (compressed).

The ffmpeg_decoder.cc file is the one you have to modify.

Like the other config files, the Makefile uses the $(NACL_SDK_ROOT) env variable.

# Misc

Here the FFMPeg compile options:
```sh
./configure \
    --target-os=linux \
    --arch=pnacl \
    --disable-runtime-cpudetect \
    --prefix="$NACL_SDK_ROOT/toolchain/linux_pnacl/le32-nacl/usr" \
    --cross-prefix=pnacl- \
    --cc=pnacl-clang \
    --ld=pnacl-clang++ \
    --pkg_config=pkg-config \
    --enable-gpl \
    --enable-static \
    --disable-shared \
    --enable-incompatible-libav-abi \
    --enable-cross-compile \
    --disable-ffplay \
    --enable-ffprobe \
    --disable-ffserver \
    --disable-asm \
    --disable-inline-asm \
    --disable-indevs \
    --disable-protocols \
    --disable-network \
    --disable-protocol=file \
    --disable-demuxer=rtsp \
    --disable-demuxer=image2 \
    --disable-doc \
    --disable-htmlpages \
    --disable-manpages \
    --disable-podpages \
    --disable-txtpages \
    --disable-network \
    --extra-libs='-lppapi_simple -lppapi -lppapi_cpp -lnacl_io' \
    --enable-libx264 \
    --disable-libx265 \
    --disable-libvpx \
    --enable-avformat \
    --enable-avdevice \
    --enable-avfilter \
    --enable-swresample \
    --enable-pthreads \
    --enable-avcodec \
	--enable-avutil \
	--enable-decoder=h264
```

autothrow
=========

Experiments in machine vision

Notes on opencv
---------------
Install x264

Much of this taken from: http://www.ozbotz.org/opencv-installation/

Dependencies (Debian, Ubuntu, Mint etc.)  Correct for Ubuntu 13.04 Raring
(aka Mint 15 Olivia)

    - build-essential
    - checkinstall
    - cmake
    - git
    - gstreamer0.10-ffmpeg
    - gstreamer0.10-plugins-bad
    - gstreamer0.10-plugins-base
    - gstreamer0.10-plugins-good
    - gstreamer0.10-plugins-ugly
    - gstreamer0.10-tools
    - libfaac-dev
    - libgstreamer0.10-0
    - libgstreamer0.10-dev
    - libgstreamer-plugins-base0.10-dev
    - libgtk2.0-0
    - libgtk2.0-dev
    - libjack-jackd2-dev
    - libjpeg8
    - libjpeg8-dev
    - libmp3lame-dev
    - libopencore-amrnb-dev
    - libopencore-amrwb-dev
    - libsdl1.2-dev
    - libtheora-dev
    - libva-dev
    - libvdpau-dev
    - libvorbis-dev
    - libx11-dev
    - libxfixes-dev
    - libxvidcore-dev
    - texi2html
    - yasm
    - zlib1g-dev

Install x264

    wget ftp://ftp.videolan.org/pub/x264/snapshots/last_x264.tar.bz2
    x264home=$HOME/apps/x264-snapshot-20131207-2245
    ./configure --enable-shared --enable-pic --prefix=${x264home}
    make && make install

Install ffmpeg

    ffmpeghome=$HOME/apps/ffmpeg-2.1.1
    ./configure --enable-gpl --enable-libfaac --enable-libmp3lame \
      --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libtheora \
      --enable-libvorbis --enable-libx264 --enable-libxvid --enable-nonfree \
      --enable-postproc --enable-version3 --enable-x11grab --enable-shared --enable-pic \
      --extra-cflags=-I${x264home}/include \
      --extra-ldflags="-L${x264home}/lib -Wl,-rpath=${x264home}/lib" \
      --prefix=${ffmpeghome}
    make && make install

Install v4l

    v4lhome=$HOME/apps/v4l-utils-1.0.0
    ./configure --prefix=${v4lhome} --with-udevdir=${v4lhome}/udev

Install opencv

    export LD_LIBRARY_PATH=${ffmpeghome}/lib
    export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:/${ffmpeghome}/lib/pkgconfig
    export PKG_CONFIG_LIBDIR=$PKG_CONFIG_LIBDIR:/${ffmpeghome}/lib

    opencv=opencv-2.4.7
    opencvhome=$HOME/apps/${opencv}

    tar xf ${opencv}.tar.gz
    mkdir -p ${opencv}/release
    cd ${opencv}/release
    cmake -DCMAKE_LIBRARY_PATH=${x264home}/lib:${ffmpeghome}/lib:${v4lhome}/lib \
          -DCMAKE_INCLUDE_PATH=${x264home}/include:${ffmpeghome}/include:${v4lhome}/include \
          -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=${opencvhome} ..
    make && make install

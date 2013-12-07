autothrow
=========

Experiments in machine vision

Notes on opencv
---------------
opencv=opencv-2.4.7
tar xf ${opencv}.tar.gz && \
mkdir -p ${opencv}/release && \
cd ${opencv}/release && \
cmake -DCMAKE_BUILD_TYPE=RELEASE -D CMAKE_INSTALL_PREFIX=../../../opencv .. && \
make && make install

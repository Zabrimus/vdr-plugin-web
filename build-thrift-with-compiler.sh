#!/bin/bash

THRIFT_VERSION=0.21.0

mkdir -p thrift-install
cd thrift-install

# download thrift
if [ ! -e v${THRIFT_VERSION}.zip ]; then
  wget https://github.com/apache/thrift/archive/refs/tags/v${THRIFT_VERSION}.zip
fi

# unpack thrift
if [ ! -d thrift-${THRIFT_VERSION} ]; then
  unzip v${THRIFT_VERSION}.zip
fi

# build thrift
mkdir -p thrift-${THRIFT_VERSION}/build-dev
cd thrift-${THRIFT_VERSION}/build-dev

cmake -DWITH_AS3=OFF \
      -DWITH_QT5=OFF \
      -DBUILD_JAVA=OFF \
      -DBUILD_JAVASCRIPT=OFF \
      -DWITH_NODEJS=OFF \
      -DBUILD_PYTHON=OFF \
      -DBUILD_TESTING=OFF \
      -DWITH_C_GLIB=OFF \
      -DWITH_OPENSSL=OFF \
      -DBUILD_SHARED_LIBS=OFF \
      -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
      -DCMAKE_INSTALL_PREFIX=../../install \
       ..

make -j 4

# installs to thrift-install/install
make install
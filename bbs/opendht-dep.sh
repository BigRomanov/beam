#!/bin/bash

pushd $HOME

mkdir -p opendht-build
cd opendht-build
if [ ! -d opendht ]; then
    git clone https://github.com/savoirfairelinux/opendht.git
    if [ $? -ne "0" ]; then
        echo "Cannot git clone"
        popd
        exit 1
    fi
    cd opendht
    git submodule init
    git submodule update
    cd ..
fi

sudo apt install libncurses5-dev libreadline-dev nettle-dev libgnutls28-dev libargon2-0-dev libmsgpack-dev

mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=$HOME -DCMAKE_BUILD_TYPE=Release -DOPENDHT_PYTHON=OFF -DOPENDHT_STATIC=OFF -DOPENDHT_SHARED=ON -DOPENDHT_LOG=ON ../opendht
if [ $? -ne "0" ]; then
    echo "cmake failed"
    popd
    exit 1
fi

make -j `nproc`
if [ $? -ne "0" ]; then
    echo "make failed"
    popd
    exit 1
fi

make install
if [ $? -ne "0" ]; then
    echo "make install failed"
    popd
    exit 1
fi

popd

echo "DONE"

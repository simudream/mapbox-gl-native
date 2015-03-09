FROM ubuntu:12.04

RUN apt-get update -y && \
    apt-get install -y build-essential git-core python-pip python-software-properties software-properties-common && \
    rm -rf /var/lib/apt/lists/*

RUN apt-get update -y && \
    apt-get -y install curl zlib1g-dev automake gdb libtool xutils-dev make cmake pkg-config python-pip libboost-dev libcurl4-openssl-dev libpng-dev libsqlite3-dev mesa-utils libxi-dev x11proto-randr-dev x11proto-xext-dev libxrandr-dev x11proto-xf86vidmode-dev libxxf86vm-dev libxcursor-dev libxinerama-dev xvfb imagemagick llvm-3.4 && \
    rm -rf /var/lib/apt/lists/* && \
    pip install awscli

RUN useradd -ms /bin/bash mapbox

USER mapbox
ENV HOME /home/mapbox
WORKDIR /home/mapbox

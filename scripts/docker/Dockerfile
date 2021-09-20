#FROM i386/ubuntu:18.04
FROM ubuntu:18.04

# Base stuff
RUN \
  apt-get update && \
  apt-get install -y software-properties-common && \
  add-apt-repository ppa:openjdk-r/ppa && \
  apt-get update && \
  apt-get install -y openjdk-11-jdk

# Using the vanilla GCC (7.5.0)

RUN \
  dpkg --add-architecture i386 && \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    libssl-dev \
    openssl \
    libtool \
    autoconf \
    automake \
    build-essential \
    uuid-dev \
    libxi-dev \
    libopenal-dev \
    libgl1-mesa-dev \
    libglw1-mesa-dev \
    freeglut3-dev \
    gcc-multilib \
    g++-multilib

RUN \
  apt-get update && apt-get install -y \
    xz-utils \
    curl \
    && rm -rf /var/lib/apt/lists/* \
    && curl -SL https://github.com/llvm/llvm-project/releases/download/llvmorg-10.0.0/clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04.tar.xz \
    | tar -xJC . && \
    mv clang+llvm-10.0.0-x86_64-linux-gnu-ubuntu-18.04 clang_10.0.0

ENV PATH=/clang_10.0.0/bin:$PATH
ENV LD_LIBRARY_PATH=/clang_10.0.0/lib:$LD_LIBRARY_PATH

RUN \
  echo "PYTHON" && \
  apt-get autoclean && \
  apt-get update && \
  apt-get install -y --no-install-recommends python2.7 python-setuptools && \
  ln -s /usr/bin/python2.7 /usr/local/bin/python && \
  ln -s /usr/bin/python2.7 /usr/local/bin/python2 && \
  sh -c "echo \#\!/usr/bin/env bash > /usr/local/bin/easy_install" && \
  sh -c "echo python /usr/lib/python2.7/dist-packages/easy_install.py $\* >> /usr/local/bin/easy_install" && \
  chmod +x /usr/local/bin/easy_install

RUN \
  apt-get install -y --no-install-recommends \
    tofrodos \
    cmake \
    curl \
    wget \
    zip \
    unzip \
    tree \
    silversearcher-ag \
    valgrind \
    git

RUN apt-get autoremove

ENV LC_ALL C.UTF-8

# Add builder user
RUN  useradd -r -u 2222 builder && \
  mkdir -p /var/builder && \
  chown builder: /var/builder && \
  chown builder: $(readlink -f /usr/bin/java) && \
  chmod +s $(readlink -f /usr/bin/java)

USER builder
WORKDIR /home/builder
RUN mkdir -p /home/builder

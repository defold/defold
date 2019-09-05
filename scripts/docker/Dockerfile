#FROM i386/ubuntu:18.04
FROM ubuntu:18.04

# Base stuff
RUN \
  apt-get update && \
  apt-get install -y software-properties-common && \
  add-apt-repository ppa:openjdk-r/ppa && \
  apt-get update && \
  apt-get install -y openjdk-11-jdk

# We still use GCC-5: https://askubuntu.com/a/1087368

RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    gcc-5 \
    g++-5 \
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
    freeglut3-dev && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 10 && \
	update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 20 && \
	update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 10 && \
	update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 20 && \
	update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30 && \
	update-alternatives --set cc /usr/bin/gcc && \
	update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30 && \
	update-alternatives --set c++ /usr/bin/g++

RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    clang-6.0 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-6.0 1000 && \
    update-alternatives --install /usr/bin/clang clang /usr/bin/clang-6.0 1000 && \
    update-alternatives --config clang && \
    update-alternatives --config clang++

RUN \
  apt-get install -y --no-install-recommends \
    tofrodos \
    cmake \
    curl \
    tree \
    silversearcher-ag \
    valgrind \
    git \
    python2.7 \
    python-setuptools && \
  ln -s /usr/bin/python2.7 /usr/local/bin/python && \
  ln -s /usr/bin/python2.7 /usr/local/bin/python2 && \
  sh -c "echo \#\!/usr/bin/env bash > /usr/local/bin/easy_install" && \
  sh -c "echo python /usr/lib/python2.7/dist-packages/easy_install.py $\* >> /usr/local/bin/easy_install" && \
  chmod +x /usr/local/bin/easy_install

RUN apt-get autoremove


# Add builder user
RUN  useradd -r -u 2222 builder && \
  mkdir -p /var/builder && \
  chown builder: /var/builder && \
  chown builder: $(readlink -f /usr/bin/java) && \
  chmod +s $(readlink -f /usr/bin/java)

USER builder
WORKDIR /home/builder
RUN mkdir -p /home/builder


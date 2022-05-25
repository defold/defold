FROM ubuntu:20.04

# Base stuff
RUN \
  apt-get update && \
  apt-get install -y software-properties-common && \
  add-apt-repository ppa:openjdk-r/ppa && \
  apt-get update && \
  apt-get install -y openjdk-11-jdk

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
    g++-multilib \
    # for python+ctypes
    libffi-dev


RUN \
  apt-get install -y --no-install-recommends \
    tofrodos \
    cmake \
    wget \
    zip \
    unzip \
    tree \
    silversearcher-ag \
    valgrind \
    git

ENV CLANG_VERSION 13.0.1
RUN \
  apt-get update && apt-get install -y \
    xz-utils \
    curl \
    libtinfo5 \
    && rm -rf /var/lib/apt/lists/* \
    && curl -SL https://github.com/llvm/llvm-project/releases/download/llvmorg-${CLANG_VERSION}/clang+llvm-${CLANG_VERSION}-x86_64-linux-gnu-ubuntu-18.04.tar.xz \
    | tar -xJC . && \
    mv clang+llvm-${CLANG_VERSION}-x86_64-linux-gnu-ubuntu-18.04 clang_${CLANG_VERSION}

ENV PATH=/clang_${CLANG_VERSION}/bin:$PATH
ENV LD_LIBRARY_PATH=/clang_${CLANG_VERSION}/lib:$LD_LIBRARY_PATH

ENV PYENV_ROOT /.pyenv
ENV PATH $PYENV_ROOT/shims:$PYENV_ROOT/bin:$PATH

ENV PYTHON3_VERSION 3.10.4

RUN \
    echo "PYENV" && \
    set -ex \
    && curl https://pyenv.run | bash \
    && pyenv update \
    && pyenv install $PYTHON3_VERSION \
    && pyenv global $PYTHON3_VERSION \
    && pyenv rehash

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

FROM ubuntu:20.04

# Base stuff
RUN \
  apt-get update && \
  apt-get install -y software-properties-common

RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    g++-aarch64-linux-gnu \
    g++-x86-64-linux-gnu \
    gcc-aarch64-linux-gnu \
    gcc-i686-linux-gnu \
    gcc-arm-linux-gnueabi \
    libssl-dev \
    openssl \
    libtool \
    autoconf \
    automake \
    build-essential \
    uuid-dev \
    libxi-dev \
    libxext-dev \
    libopenal-dev \
    libgl1-mesa-dev \
    libglw1-mesa-dev \
    freeglut3-dev \
    libx11-xcb-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libxrandr-dev \
    # for python+ctypes
    libffi-dev

RUN \
  apt-get install -y --no-install-recommends \
    tofrodos \
    wget \
    curl \
    zip \
    unzip \
    tree \
    silversearcher-ag \
    git

#
# llvm
#
ENV CLANG_VERSION=17
RUN \
  echo "LLVM + CLANG ${CLANG_VERSION}" && \
  wget https://apt.llvm.org/llvm.sh && \
  chmod +x llvm.sh && \
  ./llvm.sh ${CLANG_VERSION} && \
  rm llvm.sh

ENV PATH=/usr/lib/llvm-${CLANG_VERSION}/bin:$PATH
ENV LD_LIBRARY_PATH=/usr/lib/llvm-${CLANG_VERSION}/lib:$LD_LIBRARY_PATH

ENV PYENV_ROOT /.pyenv
ENV PATH $PYENV_ROOT/shims:$PYENV_ROOT/bin:$PATH

ENV PYTHON3_VERSION 3.13

RUN \
    echo "PYENV" && \
    set -ex \
    && curl https://pyenv.run | bash \
    && pyenv update \
    && pyenv install $PYTHON3_VERSION \
    && pyenv global $PYTHON3_VERSION \
    && pyenv rehash

ENV LC_ALL C.UTF-8

# Java
ENV JAVA_HOME=/usr/local/jdk-17.0.13+11
ENV PATH=${JAVA_HOME}/bin:${PATH}

RUN \
  mkdir -p /usr/local && \
  wget -q -O - https://aka.ms/download-jdk/microsoft-jdk-17.0.13-linux-aarch64.tar.gz | tar xz -C /usr/local && \
  java -version && \
  javac -version

# cmake
RUN \
  wget -qO- "https://cmake.org/files/v3.31/cmake-3.31.0-rc1-linux-aarch64.tar.gz" | tar --strip-components=1 -xz -C /usr/local

# go
ENV PATH=/usr/local/go/bin:${PATH}
RUN \
  wget -qO- "https://go.dev/dl/go1.23.2.linux-arm64.tar.gz" | tar -xz -C /usr/local

RUN apt-get autoremove

ENV LC_ALL C.UTF-8

# Add builder user
RUN  useradd -r -u 2222 builder && \
  mkdir -p /var/builder && \
  chown builder: /var/builder && \
  chown builder: $(readlink -f ${JAVA_HOME}/bin/java) && \
  chmod +s $(readlink -f ${JAVA_HOME}/bin/java)

USER builder
WORKDIR /home/builder
RUN mkdir -p /home/builder

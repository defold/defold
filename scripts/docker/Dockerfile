FROM ubuntu:20.04

# Base stuff
RUN \
  apt-get update && \
  apt-get install -y software-properties-common && \
  add-apt-repository ppa:openjdk-r/ppa && \
  apt-get update && \
  apt-get install -y openjdk-11-jdk

RUN \
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

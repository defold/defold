FROM ubuntu:22.04

# Base stuff
RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    software-properties-common \
    ca-certificates \
    curl \
    gnupg \
    dirmngr \
    gpg-agent

RUN \
  apt-get update && \
  install -m 0755 -d /etc/apt/keyrings && \
  curl -fsSL https://download.docker.com/linux/debian/gpg -o /etc/apt/keyrings/docker.asc && \
  chmod a+r /etc/apt/keyrings/docker.asc


RUN \
  echo "*****************" && \
  echo "Custom /etc/apt/sources.list" && \
  echo "# custom" > /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy main restricted" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-updates main restricted" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy universe" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-updates universe" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy multiverse" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-updates multiverse" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-backports main restricted universe multiverse" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-security main restricted" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-security universe" >> /etc/apt/sources.list && \
  echo "deb [arch=arm64] http://ports.ubuntu.com/ubuntu-ports/ jammy-security multiverse" >> /etc/apt/sources.list && \
  echo "*****************"

RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    gcc \
    g++ \
    g++-aarch64-linux-gnu \
    gcc-aarch64-linux-gnu \
    g++-x86-64-linux-gnu \
    gcc-x86-64-linux-gnu \
    openssl \
    libtool \
    autoconf \
    automake

RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    tofrodos \
    wget \
    curl \
    zip \
    unzip \
    tree \
    silversearcher-ag \
    git


RUN \
  dpkg --add-architecture amd64 && \
  echo "*****************" && \
  echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/docker.asc] http://archive.ubuntu.com/ubuntu/ jammy main restricted universe multiverse" >> /etc/apt/sources.list.d/docker.list && \
  echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/docker.asc] http://archive.ubuntu.com/ubuntu/ jammy-updates main restricted universe multiverse" >> /etc/apt/sources.list.d/docker.list && \
  echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/docker.asc] http://archive.ubuntu.com/ubuntu/ jammy-backports main restricted universe multiverse" >> /etc/apt/sources.list.d/docker.list && \
  echo "deb [arch=amd64 signed-by=/etc/apt/keyrings/docker.asc] http://archive.ubuntu.com/ubuntu/ jammy-security main restricted universe multiverse" >> /etc/apt/sources.list.d/docker.list && \
  echo "*****************" && \
  echo "apt-key" && \
  apt-key --keyring /etc/apt/keyrings/docker.asc adv --keyserver keyserver.ubuntu.com --recv-keys 0E98404D386FA1D9 6ED0E7B82643E131 605C66F00D6C9793 && \
  apt-key --keyring /etc/apt/keyrings/docker.asc adv --keyserver keyserver.ubuntu.com --recv-keys 3B4FE6ACC0B21F32 871920D1991BC93C && \
  echo "*****************" && \
  echo "apt-get dist-upgrade" && \
  apt-get dist-upgrade  -y --no-install-recommends && \
  echo "*****************"

RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    build-essential \
    libssl-dev \
    uuid-dev \
    uuid-dev:amd64 \
    libxi-dev \
    libxi-dev:amd64 \
    libxext-dev \
    libxext-dev:amd64 \
    libopenal-dev \
    libopenal-dev:amd64 \
    libgl1-mesa-dev \
    libglw1-mesa-dev \
    freeglut3-dev \
    freeglut3-dev:amd64 \
    libx11-xcb-dev \
    libx11-xcb-dev:amd64 \
    libxkbcommon-dev \
    libxkbcommon-dev:amd64 \
    libwayland-dev \
    libwayland-dev:amd64 \
    libxcursor-dev \
    libxcursor-dev:amd64 \
    libxinerama-dev \
    libxinerama-dev:amd64 \
    libxrandr-dev \
    libxrandr-dev:amd64 \
    # for python+ctypes
    libffi-dev



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

ENV PYENV_ROOT=/.pyenv
ENV PATH=$PYENV_ROOT/shims:$PYENV_ROOT/bin:$PATH

ENV PYTHON3_VERSION=3.13

RUN \
    echo "PYENV" && \
    set -ex \
    && curl https://pyenv.run | bash \
    && pyenv update \
    && pyenv install $PYTHON3_VERSION \
    && pyenv global $PYTHON3_VERSION \
    && pyenv rehash

ENV LC_ALL=C.UTF-8

# Java (Temurin OpenJDK 21)
# Install from official Adoptium API tarball to avoid apt signature issues
ENV JAVA_HOME=/usr/lib/jvm/temurin-21-jdk
ENV PATH=${JAVA_HOME}/bin:${PATH}

RUN set -eux; \
  echo "Installing Temurin OpenJDK 21 (tarball)"; \
  arch="$(dpkg --print-architecture)"; \
  case "$arch" in \
    arm64) jarch=aarch64 ;; \
    amd64) jarch=x64 ;; \
    *) echo "Unsupported architecture: $arch"; exit 1 ;; \
  esac; \
  url="https://api.adoptium.net/v3/binary/latest/21/ga/linux/${jarch}/jdk/hotspot/normal/eclipse?project=jdk"; \
  echo "Downloading: $url"; \
  mkdir -p /tmp/java && cd /tmp/java; \
  curl -fsSL "$url" -o openjdk.tar.gz; \
  mkdir -p "$JAVA_HOME"; \
  tar -xzf openjdk.tar.gz --strip-components=1 -C "$JAVA_HOME"; \
  cd /; rm -rf /tmp/java; \
  "$JAVA_HOME/bin/java" -version; \
  "$JAVA_HOME/bin/javac" -version

# cmake
RUN \
  wget -qO- "https://cmake.org/files/v4.1/cmake-4.1.0-linux-aarch64.tar.gz" | tar --strip-components=1 -xz -C /usr/local

# ninja (latest stable)
ENV NINJA_VERSION=1.12.1
RUN \
  echo "Installing Ninja ${NINJA_VERSION} from source" && \
  git clone --depth 1 --branch v${NINJA_VERSION} https://github.com/ninja-build/ninja.git /tmp/ninja && \
  cd /tmp/ninja && \
  python configure.py --bootstrap && \
  install -m 0755 ninja /usr/local/bin/ninja && \
  cd / && rm -rf /tmp/ninja && \
  ninja --version

# go
ENV PATH=/usr/local/go/bin:${PATH}
RUN \
  wget -qO- "https://go.dev/dl/go1.23.2.linux-arm64.tar.gz" | tar -xz -C /usr/local

RUN apt-get autoremove

ENV LC_ALL=C.UTF-8

# Add builder user
RUN  useradd -r -u 2222 builder && \
  mkdir -p /var/builder && \
  chown builder: /var/builder && \
  chown builder: $(readlink -f ${JAVA_HOME}/bin/java) && \
  chmod +s $(readlink -f ${JAVA_HOME}/bin/java)

USER builder
WORKDIR /home/builder
RUN mkdir -p /home/builder

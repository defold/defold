FROM ubuntu:22.04

ARG TARGETARCH

ENV JDK_ARCH=${TARGETARCH/amd64/x64}
ENV JDK_ARCH=${JDK_ARCH/arm64/aarch64}

ENV CMAKE_ARCH=${TARGETARCH/amd64/x86_64}
ENV CMAKE_ARCH=${CMAKE_ARCH/arm64/aarch64}

ENV LC_ALL=C.UTF-8


RUN \
  apt-get update && \
  apt-get install -y --no-install-recommends \
    software-properties-common \
    ca-certificates \
    curl \
    gnupg \
    dirmngr \
    gpg-agent \
    gcc \
    g++ \
    openssl \
    libtool \
    autoconf \
    automake \
    tofrodos \
    wget \
    curl \
    zip \
    unzip \
    tree \
    ripgrep \
    git \
    build-essential \
    libssl-dev \
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
    libxcursor-dev \
    libxinerama-dev \
    libxrandr-dev \
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


# Java (Temurin OpenJDK 25)
# Install from official Adoptium API tarball to avoid apt signature issues
ENV JAVA_HOME=/usr/lib/jvm/temurin-25-jdk
ENV PATH=${JAVA_HOME}/bin:${PATH}

RUN set -eux; \
  echo "Installing Temurin OpenJDK 25 (tarball)"; \
  arch="$(dpkg --print-architecture)"; \
  url="https://api.adoptium.net/v3/binary/latest/25/ga/linux/${JDK_ARCH}/jdk/hotspot/normal/eclipse?project=jdk"; \
  echo "Downloading: $url"; \
  mkdir -p /tmp/java && cd /tmp/java; \
  curl -fsSL "$url" -o openjdk.tar.gz; \
  mkdir -p "$JAVA_HOME"; \
  tar -xzf openjdk.tar.gz --strip-components=1 -C "$JAVA_HOME"; \
  cd /; rm -rf /tmp/java; \
  "$JAVA_HOME/bin/java" -version; \
  "$JAVA_HOME/bin/javac" -version

# cmake
ENV CMAKE_VERSION=4.1.1
RUN \
  wget -qO- "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-${CMAKE_ARCH}.tar.gz" | tar --strip-components=1 -xz -C /usr/local

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
  wget -qO- "https://go.dev/dl/go1.23.2.linux-${TARGETARCH}.tar.gz" | tar -xz -C /usr/local

RUN apt-get autoremove

# Add builder user
RUN  useradd -r -u 2222 builder && \
  mkdir -p /var/builder && \
  chown builder: /var/builder && \
  chown builder: $(readlink -f ${JAVA_HOME}/bin/java) && \
  chmod +s $(readlink -f ${JAVA_HOME}/bin/java)

USER builder
WORKDIR /home/builder
RUN mkdir -p /home/builder

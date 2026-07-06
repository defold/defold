#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.



# common tools
sudo apt install git 

sudo apt-get install -y --no-install-recommends \
	libssl-dev \
	openssl \
	libtool \
	autoconf \
	automake \
	uuid-dev \
	libxi-dev \
	libopenal-dev \
	libgl1-mesa-dev \
	libglw1-mesa-dev \
	freeglut3-dev \
	libtinfo5 \
	unzip \
	zip \
	# for use when debugging
	tree

# # Java 11

sudo apt-get install -y software-properties-common
sudo add-apt-repository -y ppa:openjdk-r/ppa
sudo apt-get update -y
sudo apt-get install -y --no-install-recommends openjdk-11-jdk

echo 'export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64' >> $HOME/.bashrc
echo 'export PATH=$JAVA_HOME/bin:$PATH' >> $HOME/.bashrc

export JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64
export PATH=$JAVA_HOME/bin:$PATH

# javac -version

# Clang

wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 13
rm llvm.sh

echo 'export PATH=/usr/lib/llvm-13/bin:$PATH' >> $HOME/.bashrc
export PATH=/usr/lib/llvm-13/bin:$PATH

# clang++ --version

# # ripgrep
RIPGREP_VERSION=$(curl -s "https://api.github.com/repos/BurntSushi/ripgrep/releases/latest" | grep -Po '"tag_name": "\K[0-9.]+')
curl -Lo ripgrep.deb "https://github.com/BurntSushi/ripgrep/releases/latest/download/ripgrep_${RIPGREP_VERSION}_amd64.deb"
sudo apt install -y ./ripgrep.deb
rg --version
rm ./ripgrep.deb

# python 2.7
sudo apt install -y --no-install-recommends python2
ln -s /usr/bin/python2 /usr/bin/python

sudo apt-get clean autoclean autoremove

# X11 port forward

echo "export DISPLAY=localhost:0.0" >> $HOME/.bashrc

# output some diagnostics
echo "*****************************"
git --version
echo "JAVA_HOME=$JAVA_HOME"
javac -version
rg --version
clang++ --version
python --version

#!/usr/bin/env python

import os, sys, shutil, zipfile, re, itertools, json, platform, math, mimetypes
import optparse, subprocess, urllib, urlparse, tempfile, time
import imp
from datetime import datetime
from tarfile import TarFile
from os.path import join, dirname, basename, relpath, expanduser, normpath, abspath
from glob import glob
from threading import Thread, Event
from Queue import Queue
from ConfigParser import ConfigParser
from build import download_sdk

PACKAGES_NX64="protobuf-2.3.0".split()
PACKAGE_NX64_SDK="nx64-sdk-10.1"


def get_target_platforms():
    return ['arm64-nx64']

def get_install_host_packages(platform):
    """ Returns the packages that should be installed for the host
    """
    return []

def get_install_target_packages(platform):
    """ Returns the packages that should be installed for the host
    """
    if platform == 'arm64-nx64':    return PACKAGES_NX64
    return []

def install_sdk(self, platform):
    """ Installs the sdk for the private platform
    """
    if platform in ('arm64-nx64',):
        parent_folder = join(self.ext, 'SDKs', 'nx64')
        print "download_sdk!", join(parent_folder, PACKAGE_NX64_SDK), os.path.exists(join(parent_folder, PACKAGE_NX64_SDK))
        download_sdk(self, '%s/%s.tar.gz' % (self.package_path, PACKAGE_NX64_SDK), join(parent_folder, PACKAGE_NX64_SDK), strip_components=0)

def is_library_supported(platform, library):
    if platform == 'arm64-nx64':
        return library not in ['glfw']
    return True

#! /usr/bin/env python
# Copyright 2020-2025 The Defold Foundation
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

from minio import Minio
from minio.error import S3Error
from minio.sse import SseCustomerKey
import os.path, sys

class DownloadProgress:
    def __init__(self, progress_callback):
        self.__callback = progress_callback
        self.__downloaded_object = None
        self.__total_size = 0
        self.__downloaded_bytes = 0


    def set_meta(self, object_name, total_length):
        self.__downloaded_object = object_name
        self.__total_size = total_length

    def update(self, length):
        self.__downloaded_bytes += length
        if self.__callback is not None:
            self.__callback(self.__downloaded_object, self.__downloaded_bytes, self.__total_size)


class MinioClient:
    def __init__(self, base_url, bucket, access_key = None, access_secret = None, encryption_key = None, download_dir = ".packages_downloads"):
        self.__base_url = base_url
        self.__bucket_name = bucket
        self.__access_key = access_key
        self.__access_secret = access_secret
        self.__encryption_key = encryption_key
        self.__download_dir = download_dir
        self.__client = None

    def __initialize(self):
        if self.__base_url is None:
            print("No package path provided. Use either --package-base-url option or DM_PACKAGES_BASE_URL environment variable")
            sys.exit(1)
        if os.path.isdir(self.__base_url):
            self.__is_local_path = True
            self.__base_dir = self.__base_url
        else:
            self.__is_local_path = False
            self.__client = Minio(self.__base_url, access_key=self.__access_key, secret_key=self.__access_secret)
            self.__encryption_key = SseCustomerKey(bytes.fromhex(self.__encryption_key)) if self.__encryption_key else None
        self.__target_dir = self.__download_dir
        os.makedirs(self.__download_dir, exist_ok=True)

    def __get_local_package(self, package_name):
        path = os.path.join(self.__base_dir, package_name)
        if os.path.exists(os.path.join()):
            return os.path.normpath(os.path.abspath(path))
        print("Could not find local file:", path)
        sys.exit(1)


    def download_package(self, package_name, progress_cb):
        if self.__client is None:
            self.__initialize()
        if self.__is_local_path:
            return self.__get_local_package(package_name)
        response = None
        try:
            result_path = os.path.join(self.__target_dir, package_name)
            self.__client.fget_object(self.__bucket_name, package_name, result_path, ssec=self.__encryption_key, progress=DownloadProgress(progress_cb))
            return os.path.abspath(result_path)
        except S3Error as err:
            print("Error during SDK downloading: ", err)
        finally:
            if response:
                response.close()
                response.release_conn()
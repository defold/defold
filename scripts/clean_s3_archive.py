#!/usr/bin/env python
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


import s3

bucket_name = "d.defold.com"
archive_root = "archive"
bucket = s3.get_bucket(bucket_name)

keep = [
    "5791ee6d96b87e50eee5acd70abaa4026fefef28", #1.2.170
    "4ebe7a1d548eae2398717ed46f9d7d1b103d5503", #1.2.169
    "e22f6d2f81e7c53ebcbfefe703ff22ce5da252c0", #1.2.168
    "96f7a5e4f617d5f6f4645f30a3e6ff656689435d", #1.2.167
    "5295afb3878441fb12f497df8831148525dcfb10", #1.2.166
    "6fac6e80f09ab297093e3ff65a7f45ad56e06e33", #1.2.165
    "ff34def383f372b1f302916374310bd498105384", #1.2.171 beta
    "a98007b48691529b59fb099fc369d81518059d00", #1.2.171 editor-alpha (stable)
    "stable",
    "beta",
    "alpha",
    "dev",
    "editor-alpha"
]

for object_summary in bucket.objects.filter(Prefix=archive_root):
    objects_to_delete = []
    parts = object_summary.key.split("/")
    sha1 = parts[1]
    if sha1 not in keep:
        print("Schedule for deleting %s" % object_summary.key)
        objects_to_delete.append({'Key': object_summary.key})
    bucket.delete_objects(
        Delete={
            'Objects': objects_to_delete
        }
    )

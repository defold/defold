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

from log import log
import os
from os.path import join
import re
import run
import tempfile
import unpack

def upload(build_root, build_version, app_launch):
	cloud_dir = tempfile.mkdtemp()

	cmd = ['BuildPatchTool',
		'-OrganizationId="o-gj7ywfbpzulctvkj7evxbfrpxtkng8"',
		'-ProductId="eb03b236cc34490c9ecd59f60aa143fd"',
		'-ArtifactId="37bdd03a131a4ced814926c84e74a4e3"',
		'-ClientId="xyza789122DEs9Ph5OfvSz6yxriOpRZl"',
		'-ClientSecretEnvVar="DEFOLD_EGS_BPT_CLIENT_SECRET"',
		'-mode=UploadBinary',
		'-BuildRoot="%s"' % build_root,
		'-CloudDir="%s"' % cloud_dir,
		'-BuildVersion="%s"' % build_version,
		'-AppLaunch="%s"' % app_launch,
		'-AppArgs=""']

	run.shell_command(" ".join(cmd))

def label(build_version, platform):
	cmd = ['BuildPatchTool',
		'-OrganizationId="o-gj7ywfbpzulctvkj7evxbfrpxtkng8"',
		'-ProductId="eb03b236cc34490c9ecd59f60aa143fd"',
		'-ArtifactId="37bdd03a131a4ced814926c84e74a4e3"',
		'-ClientId="xyza789122DEs9Ph5OfvSz6yxriOpRZl"',
		'-ClientSecretEnvVar="DEFOLD_EGS_BPT_CLIENT_SECRET"',
		'-mode=LabelBinary',
		'-BuildVersion="%s"' % build_version,
		'-Label="Live"',
		'-Platform="%s"' % platform]

	run.shell_command(" ".join(cmd))


def release(config, urls, tag_name):
	log("Releasing Defold to Epic Game Store")

	# create dir from where we upload to egs
	upload_dir = tempfile.mkdtemp()
	linux_dir = os.path.join(upload_dir, "x86_64-linux")
	windows_dir = os.path.join(upload_dir, "x86_64-win32")
	macos_dir = os.path.join(upload_dir, "x86_64-macos")
	os.makedirs(linux_dir)
	os.makedirs(windows_dir)
	os.makedirs(macos_dir)

	# download, unpack and upload editor files
	for download_url in urls:
		filepath = config._download(download_url)
		if "x86_64-win32" in filepath:
			unpack.zip(filepath, windows_dir)
			build_version = "%s-%s" % (tag_name, "x86_64-win32")
			upload(windows_dir, build_version, "Defold/Defold.exe")
			label(build_version, "Windows")
		elif "x86_64-macos" in filepath:
			unpack.dmg(filepath, macos_dir)
			build_version = "%s-%s" % (tag_name, "x86_64-macos")
			upload(macos_dir, build_version, "Defold.app/Contents/MacOS/Defold")
			label(build_version, "Mac")
		# elif "x86_64-linux" in filepath:
		# 	unpack.zip(filepath, linux_dir)
		# 	build_version = "%s-%s" % (tag_name, "x86_64-linux")
		# 	upload(linux_dir, build_version, "Defold/Defold", "Linux")

	log("Released Defold to Epic Game Store")


if __name__ == '__main__':
	print("For testing only")
	print(get_current_repo())

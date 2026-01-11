// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.dynamo.bob.bundle;

import java.io.File;
import java.io.IOException;
import java.io.FileOutputStream;

import org.apache.commons.io.IOUtils;

import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipOutputStream;

import com.dynamo.bob.util.FileUtil;


public class ZipUtil {


	// baseDir:		/absolute/path/to/base/dir
	// file:		/absolute/path/to/base/dir/some/dir/in/zip/foo.bar
	// result:		some/dir/in/zip/foo.bar
	private static String stripBaseDir(File baseDir, File file) {
		String absoluteBase = baseDir.getAbsolutePath();
		if (!absoluteBase.endsWith(File.separator)) {
			absoluteBase = absoluteBase + File.separator;
		}
		String absoluteFile = file.getAbsolutePath();
		String strippedPath = absoluteFile.replace(absoluteBase, "");
		return strippedPath;
	}

	private static void zipFile(ZipOutputStream zipOut, File baseDir, File file) throws IOException {
		final String filePath = stripBaseDir(baseDir, file).replace('\\', '/');
		final long fileSize = file.length();

		ZipEntry ze = new ZipEntry(filePath);
		ze.setSize(fileSize);

		// Some files need to be STORED instead of DEFLATED to
		// get "correct" memory mapping at runtime.
		boolean isAsset = filePath.startsWith("assets");
		if (isAsset) {
			// Set up an uncompressed file, unfortunately need to calculate crc32 and other data for this to work.
			// https://www.infoworld.com/article/2071337/creating-zip-and-jar-files.html
			CRC32 crc = FileUtil.calculateCrc32(file);
			ze.setCrc(crc.getValue());
			ze.setCompressedSize(fileSize);
		}
		ze.setMethod(isAsset ? ZipEntry.STORED : ZipEntry.DEFLATED);

		zipOut.putNextEntry(ze);
		FileUtil.writeToStream(file, zipOut);
		zipOut.closeEntry();
	}

	private static void zipDir(ZipOutputStream zipOut, File baseDir, File dir, ICanceled canceled) throws IOException {
		for (File f : dir.listFiles()) {
			if (f.isDirectory()) {
				zipDir(zipOut, baseDir, f, canceled);
			}
			else {
				zipFile(zipOut, baseDir, f);
			}
			BundleHelper.throwIfCanceled(canceled);
		}
	}

	/**
	* Zip a all files and folders (recursively) in a dir
	*/
	public static void zipDirRecursive(File inDir, File outFile, ICanceled canceled) throws IOException {
		ZipOutputStream zipOut = null;
		try {
			zipOut = new ZipOutputStream(new FileOutputStream(outFile));
			zipDir(zipOut, inDir, inDir, canceled);
		}
		finally {
			IOUtils.closeQuietly(zipOut);
		}
	}
}

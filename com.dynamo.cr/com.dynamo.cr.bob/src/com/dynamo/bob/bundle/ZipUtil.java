package com.dynamo.bob.bundle;

import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.io.ByteArrayOutputStream;
import java.io.FileOutputStream;
import java.io.FileInputStream;

import org.apache.commons.io.IOUtils;

import java.util.zip.CRC32;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;


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
		final String filePath = stripBaseDir(baseDir, file);
		final long fileSize = file.length();

		ZipEntry ze = new ZipEntry(filePath);
		ze.setSize(fileSize);
		byte[] entryData = null;
		CRC32 crc = null;

		// Some files need to be STORED instead of DEFLATED to
		// get "correct" memory mapping at runtime.
		int zipMethod = ZipEntry.DEFLATED;
		boolean isAsset = filePath.startsWith("assets");
		if (isAsset) {
			// Set up an uncompressed file, unfortunately need to calculate crc32 and other data for this to work.
			// https://blogs.oracle.com/CoreJavaTechTips/entry/creating_zip_and_jar_files
			crc = new CRC32();
			zipMethod = ZipEntry.STORED;
			ze.setCompressedSize(fileSize);
		}

		ByteArrayOutputStream byteOut = new ByteArrayOutputStream();
		try {
			if (fileSize > 0) {
				int count;
				entryData = new byte[(int) fileSize];
				InputStream stream = new FileInputStream(file);
				while((count = stream.read(entryData, 0, (int)fileSize)) != -1) {
					byteOut.write(entryData, 0, count);
					if (zipMethod == ZipEntry.STORED) {
						crc.update(entryData, 0, count);
					}
				}
			}
		} finally {
			if(null != byteOut) {
				byteOut.close();
				entryData = byteOut.toByteArray();
			}
		}

		if (zipMethod == ZipEntry.STORED) {
			ze.setCrc(crc.getValue());
			ze.setMethod(zipMethod);
		}

		zipOut.putNextEntry(ze);
		zipOut.write(entryData);
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

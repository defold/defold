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

package com.dynamo.bob.util;

import java.io.File;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.IOException;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.util.zip.Checksum;
import java.util.zip.CRC32;
import java.nio.file.Path;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

import org.apache.commons.io.IOUtils;
import org.apache.commons.io.FileUtils;

public class FileUtil {

	private interface StreamReaderCallback {
		public void onBytes(byte[] bytes, int length) throws IOException;
	}

	private static void readFile(File file, StreamReaderCallback cb) throws IOException {
		final int bufferSize = 16 * 1024;
		byte[] bytes = new byte[bufferSize];
		int length;
		InputStream is = new BufferedInputStream(new FileInputStream(file), bufferSize);
		while((length = is.read(bytes)) != -1) {
			if (length > 0) {
				cb.onBytes(bytes, length);
			}
		}
		IOUtils.closeQuietly(is);
	}

	public static void writeToStream(File file, OutputStream out) throws IOException {
		readFile(file, new StreamReaderCallback() {
			public void onBytes(byte[] bytes, int length) throws IOException {
				out.write(bytes, 0, length);
			}
		});
	}

	public static void updateChecksum(File file, Checksum checksum) throws IOException {
		readFile(file, new StreamReaderCallback() {
			public void onBytes(byte[] bytes, int length) throws IOException {
				checksum.update(bytes, 0, length);
			}
		});
	}

	public static void updateDigest(File file, MessageDigest digest) throws IOException {
		readFile(file, new StreamReaderCallback() {
			public void onBytes(byte[] bytes, int length) throws IOException {
				digest.update(bytes, 0, length);
			}
		});
	}

	public static CRC32 calculateCrc32(File file) throws IOException {
		CRC32 crc = new CRC32();
		updateChecksum(file, crc);
		return crc;
	}

	public static byte[] calculateSha1(File file) throws Exception, NoSuchAlgorithmException {
		MessageDigest digest = MessageDigest.getInstance("SHA1");
		updateDigest(file, digest);
		return digest.digest();
	}

    public static void deleteOnExit(Path path) {
        File f = path.toFile();
        deleteOnExit(f);
    }

    public static void deleteOnExit(File f) {
        if (f.isDirectory()) {
            Runtime.getRuntime().addShutdownHook(new Thread(() -> FileUtils.deleteQuietly(f)));
        } else {
            f.deleteOnExit();
        }
    }

}
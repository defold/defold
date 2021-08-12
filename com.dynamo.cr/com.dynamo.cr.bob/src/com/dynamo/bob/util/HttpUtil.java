package com.dynamo.bob.util;

import java.io.File;
import java.io.InputStream;
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.net.URL;
import java.net.HttpURLConnection;
import java.net.ConnectException;
import java.net.ProtocolException;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;

import com.dynamo.bob.CompileExceptionError;


public class HttpUtil {

	private static void logWarning(String fmt, Object... args) {
        System.err.println(String.format(fmt, args));
    }
    private static void logInfo(String fmt, Object... args) {
        System.out.println(String.format(fmt, args));
    }

	public static void downloadToFile(URL url, File file) {
		try {
			HttpURLConnection connection = (HttpURLConnection) url.openConnection();
			connection.connect();
			int code = connection.getResponseCode();

			if (code == 304) {
				logInfo("Status %d: Already cached", code);
			}
			else if (code >= 400) {
				logWarning("Status %d: Failed to download %s", code, url);
				throw new RuntimeException(String.format("Status %d: Failed to download %s", code, url), new Exception());
			}
			else {
				InputStream input = new BufferedInputStream(connection.getInputStream());
				FileUtils.copyInputStreamToFile(input, file);
				IOUtils.closeQuietly(input);
			}
			connection.disconnect();
		}
		catch (ConnectException e) {
			throw new RuntimeException(String.format("Connection refused by the server at %s", url.toString()), e);
		}
		catch (FileNotFoundException e) {
			throw new RuntimeException(String.format("The URL %s points to a resource which doesn't exist", url.toString()), e);
		}
		catch (IOException e) {
			throw new RuntimeException(String.format("Connection refused by the server at %s", url.toString()), e);
		}
	}

	public static void uploadFile(URL url, File file) {
		try {
			HttpURLConnection connection = (HttpURLConnection) url.openConnection();
			connection.setRequestMethod("PUT");
			connection.setRequestProperty("Content-type", "application/octet-stream");
			connection.setDoOutput(true);
			connection.connect();

			BufferedOutputStream bos = new BufferedOutputStream(connection.getOutputStream());
			BufferedInputStream bis = new BufferedInputStream(new FileInputStream(file));
			byte[] buffer = new byte[4096];
			int i;
			while ((i = bis.read(buffer)) > 0) {
				bos.write(buffer, 0, i);
			}
			IOUtils.closeQuietly(bis);
			IOUtils.closeQuietly(bos);

			int code = connection.getResponseCode();
			if ((code < 200) || (code > 202)) {
				throw new RuntimeException(String.format("Status %d: Failed to upload %s", code, url), new Exception());
			}
			connection.disconnect();
		}
		catch (ConnectException e) {
			throw new RuntimeException(String.format("Connection refused by the server at %s", url.toString()), e);
		}
		catch (IOException e) {
			throw new RuntimeException(String.format("Error while uploading file %s to %s", file.toString(), url.toString()), e);
		}
	}

	public static boolean exists(URL url) {
		try {
			HttpURLConnection connection = (HttpURLConnection) url.openConnection();
			connection.setRequestMethod("HEAD");
			connection.connect();
			int code = connection.getResponseCode();
			connection.disconnect();
			return (code >= 200 && code < 400);
		}
		catch (ConnectException e) {
			throw new RuntimeException(String.format("Connection refused by the server at %s", url.toString()), e);
		}
		catch (FileNotFoundException e) {
			throw new RuntimeException(String.format("The URL %s points to a resource which doesn't exist", url.toString()), e);
		}
		catch (IOException e) {
			throw new RuntimeException(String.format("Connection refused by the server at %s", url.toString()), e);
		}
	}
}

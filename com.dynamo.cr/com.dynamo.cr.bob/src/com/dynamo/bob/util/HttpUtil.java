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
import java.io.FileInputStream;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.Map;
import java.util.HashMap;
import java.util.Base64;
import java.net.URL;
import java.net.HttpURLConnection;
import java.net.ConnectException;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.IOUtils;


public class HttpUtil {

	private Map<String, String> headers = new HashMap<>();

	public HttpUtil() {}

	private void logWarning(String fmt, Object... args) {
		System.err.println(String.format(fmt, args));
	}
	private void logInfo(String fmt, Object... args) {
		System.out.println(String.format(fmt, args));
	}

	public void setHeader(String key, String value) {
		headers.put(key, value);
	}

	public void setAuthentication(String user, String pass) {
		String userAndPass = user + ":" + pass;
		String authorization = "Basic " + Base64.getEncoder().encodeToString(userAndPass.getBytes());
		setHeader("Authorization", authorization);
	}

	private HttpURLConnection openConnection(URL url, String method) throws IOException {
		HttpURLConnection connection = (HttpURLConnection) url.openConnection();
		connection.setRequestMethod(method);
		for (String key : headers.keySet()) {
			String value = headers.get(key);
			connection.setRequestProperty(key, value);
		}
		return connection;
	}

	public void downloadToFile(URL url, File file) {
		try {
			HttpURLConnection connection = openConnection(url, "GET");
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

	public void uploadFile(URL url, File file) {
		try {
			HttpURLConnection connection = openConnection(url, "PUT");
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

	public boolean exists(URL url) {
		try {
			HttpURLConnection connection = openConnection(url, "HEAD");
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

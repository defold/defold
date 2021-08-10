package com.dynamo.bob.cache;

import java.io.File;
import java.io.IOException;
import java.nio.file.Files;
import com.dynamo.bob.fs.IResource;

public class ResourceCache {

	private String localCacheDir;

	private String remoteCacheUrl;

	private boolean enabled = false;

	public ResourceCache() {}

	public void init(String localCacheDir, String remoteCacheUrl) {
		this.localCacheDir = localCacheDir;
		this.remoteCacheUrl = remoteCacheUrl;
		this.enabled = localCacheDir != null;
	}

	private File getFile(String key) {
		return new File(localCacheDir, key);
	}

	public boolean put(String key, byte[] content) {
		if (!enabled) {
			return false;
		}
		File file = getFile(key);
		if (!file.exists()) {
			try {
				Files.write(file.toPath(), content);
			}
			catch(IOException e) {
				System.out.println("Failed to cache resource with key " + key);
				e.printStackTrace();
				return false;
			}
		}
		return true;
	}

	public byte[] get(String key) {
		if (!enabled) {
			return null;
		}
		File file = getFile(key);
		if (file.exists()) {
			try {
				return Files.readAllBytes(file.toPath());
			}
			catch(IOException e) {
				System.out.println("Failed to get resource with key " + key);
				e.printStackTrace();
			}
		}
		return null;
	}

	public boolean contains(String key) {
		if (!enabled) {
			return false;
		}
		return getFile(key).exists();
	}
}

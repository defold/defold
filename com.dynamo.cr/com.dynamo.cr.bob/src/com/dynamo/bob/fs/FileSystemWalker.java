package com.dynamo.bob.fs;

import java.util.Collection;

import com.dynamo.bob.fs.IFileSystem.IWalker;

public class FileSystemWalker implements IWalker{

    @Override
    public boolean handleDirectory(String path, Collection<String> results) {
        return true;
    }

    @Override
    public void handleFile(String path, Collection<String> results) {
        results.add(path);
    }
}

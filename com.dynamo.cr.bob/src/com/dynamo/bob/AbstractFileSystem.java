package com.dynamo.bob;

import static org.apache.commons.io.FilenameUtils.normalize;

import java.util.HashMap;
import java.util.Map;

public abstract class AbstractFileSystem<F extends IFileSystem, R extends IResource> implements IFileSystem {
    protected F fileSystem;
    protected String rootDirectory;
    protected String buildDirectory;
    protected Map<String, R> resources = new HashMap<String, R>();

    @SuppressWarnings("unchecked")
    public AbstractFileSystem() {
        fileSystem = (F) this;
    }

    @Override
    public void setRootDirectory(String rootDirectory) {
        this.rootDirectory = rootDirectory;
    }

    @Override
    public String getRootDirectory() {
        return rootDirectory;
    }

    @Override
    public void setBuildDirectory(String buildDirectory) {
        buildDirectory = normalize(buildDirectory, true);
        if (buildDirectory.startsWith("/")) {
            throw new IllegalArgumentException("Build directory must be relative to root directory and not absolute.");
        }
        this.buildDirectory = buildDirectory;
    }

    public String getBuildDirectory() {
        return buildDirectory;
    }
}

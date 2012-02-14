package com.dynamo.bob;

import java.util.HashMap;
import java.util.Map;

public abstract class AbstractFileSystem<F extends IFileSystem, R extends IResource> implements IFileSystem {
    protected F fileSystem;
    protected String buildDirectory;
    protected Map<String, R> resources = new HashMap<String, R>();

    @SuppressWarnings("unchecked")
    public AbstractFileSystem() {
        fileSystem = (F) this;
    }

    @Override
    public void setBuildDirectory(String buildDirectory) {
        this.buildDirectory = buildDirectory;
    }

    public String getBuildDirectory() {
        return buildDirectory;
    }
}

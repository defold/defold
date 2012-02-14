package com.dynamo.bob;


public class DefaultFileSystem extends AbstractFileSystem<DefaultFileSystem, DefaultResource> {

    @Override
    public IResource get(String path) {
        return new DefaultResource(this, path);
    }

}

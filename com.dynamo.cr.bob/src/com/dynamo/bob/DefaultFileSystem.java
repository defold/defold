package com.dynamo.bob;


public class DefaultFileSystem extends AbstractFileSystem<DefaultFileSystem, DefaultResource> {

    @Override
    public IResource get(String path) {
        // Paths are always root relative.
        if (path.startsWith("/"))
            path = path.substring(1);

        return new DefaultResource(this, path);
    }

}

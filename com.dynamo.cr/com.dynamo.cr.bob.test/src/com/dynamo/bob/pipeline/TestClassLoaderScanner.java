package com.dynamo.bob.pipeline;

import java.io.IOException;
import java.net.URL;

import org.eclipse.core.runtime.FileLocator;

import com.dynamo.bob.ClassLoaderScanner;

public class TestClassLoaderScanner extends ClassLoaderScanner {

    protected URL resolveURL(URL url) throws IOException {
        String proto = url.getProtocol();
        if (proto.equals("bundleresource")) {
            return FileLocator.resolve(url);
        }
        return url;
    }

}

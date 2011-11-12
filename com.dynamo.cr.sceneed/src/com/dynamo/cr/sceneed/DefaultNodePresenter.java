package com.dynamo.cr.sceneed;

import java.io.IOException;
import java.io.InputStream;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;

public class DefaultNodePresenter extends NodePresenter {

    @Override
    public Node load(String type, InputStream contents) throws IOException, CoreException {
        throw new UnsupportedOperationException();
    }

    @Override
    public Node create(String type) throws IOException, CoreException {
        throw new UnsupportedOperationException();
    }

}

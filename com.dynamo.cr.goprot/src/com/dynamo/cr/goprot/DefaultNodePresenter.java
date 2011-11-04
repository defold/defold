package com.dynamo.cr.goprot;

import java.io.IOException;
import java.io.InputStream;

import com.dynamo.cr.goprot.core.INodeView;
import com.dynamo.cr.goprot.core.NodeModel;
import com.dynamo.cr.goprot.core.NodePresenter;
import com.google.inject.Inject;

public class DefaultNodePresenter extends NodePresenter {

    @Inject
    public DefaultNodePresenter(NodeModel model, INodeView view) {
        super(model, view);
    }

    @Override
    public void onLoad(InputStream contents) throws IOException {
        throw new UnsupportedOperationException();
    }

}

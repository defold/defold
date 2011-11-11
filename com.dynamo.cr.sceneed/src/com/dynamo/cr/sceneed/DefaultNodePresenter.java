package com.dynamo.cr.sceneed;

import java.io.IOException;
import java.io.InputStream;

import com.dynamo.cr.sceneed.core.INodeView;
import com.dynamo.cr.sceneed.core.NodeModel;
import com.dynamo.cr.sceneed.core.NodePresenter;
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

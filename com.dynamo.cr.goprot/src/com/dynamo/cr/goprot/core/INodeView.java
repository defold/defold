package com.dynamo.cr.goprot.core;

import java.io.IOException;
import java.io.InputStream;

public interface INodeView {

    public interface Presenter {
        void onSelect(Node node);

        void onLoad(InputStream contents) throws IOException;
    }

    void updateNode(Node node);
}

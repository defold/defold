package com.dynamo.cr.sceneed;

import java.util.HashMap;
import java.util.Map;

import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;

public class NodeManager {
    private static class NodeImpl {
        public NodePresenter presenter;
        public INodeRenderer renderer;

        public NodeImpl(NodePresenter presenter, INodeRenderer renderer) {
            this.presenter = presenter;
            this.renderer = renderer;
        }
    }

    private Map<String, NodeImpl> typeToImpl;
    private Map<Class<? extends Node>, NodeImpl> classToImpl;

    public NodeManager() {
        this.typeToImpl = new HashMap<String, NodeImpl>();
        this.classToImpl = new HashMap<Class<? extends Node>, NodeImpl>();
    }

    public void registerNodeType(String type, Class<? extends Node> c, NodePresenter presenter, INodeRenderer renderer) {
        NodeImpl impl = new NodeImpl(presenter, renderer);
        this.typeToImpl.put(type, impl);
        this.classToImpl.put(c, impl);
    }

    public NodePresenter getPresenter(String type) {
        NodeImpl impl = this.typeToImpl.get(type);
        if (impl != null) {
            return impl.presenter;
        } else {
            return null;
        }
    }

    public NodePresenter getPresenter(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.presenter;
        } else {
            return null;
        }
    }

    public INodeRenderer getRenderer(String type) {
        NodeImpl impl = this.typeToImpl.get(type);
        if (impl != null) {
            return impl.renderer;
        } else {
            return null;
        }
    }

    public INodeRenderer getRenderer(Class<? extends Node> c) {
        NodeImpl impl = this.classToImpl.get(c);
        if (impl != null) {
            return impl.renderer;
        } else {
            return null;
        }
    }
}

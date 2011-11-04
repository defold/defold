package com.dynamo.cr.goprot;

import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

import com.dynamo.cr.goprot.core.Node;

public interface INodeOutlinePage extends IContentOutlinePage {

    public void setInput(Node node);

    public void update(Node node);

}
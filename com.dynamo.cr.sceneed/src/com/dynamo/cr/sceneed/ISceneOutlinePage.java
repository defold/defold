package com.dynamo.cr.sceneed;

import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

import com.dynamo.cr.sceneed.core.Node;

public interface ISceneOutlinePage extends IContentOutlinePage {

    public void setInput(Node node);

    public void update(Node node);

}
package com.dynamo.cr.sceneed.ui;

import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

import com.dynamo.cr.sceneed.core.Node;

public interface ISceneOutlinePage extends IContentOutlinePage {

    public void setInput(Node node);

    public void refresh();

    public void setUpdateSelection(boolean updateSelection);

}
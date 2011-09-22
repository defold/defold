package com.dynamo.cr.tileeditor;

import java.util.List;

import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

import com.dynamo.cr.tileeditor.core.Layer;

public interface IGridEditorOutlinePage extends IContentOutlinePage {

    public void setInput(List<Layer> layers, int selectedLayer);

    public void refresh();

}
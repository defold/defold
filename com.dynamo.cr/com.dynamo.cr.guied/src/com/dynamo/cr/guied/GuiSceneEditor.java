package com.dynamo.cr.guied;

import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;

import com.dynamo.cr.sceneed.ui.SceneEditor;
import com.dynamo.cr.sceneed.ui.SceneOutlinePage;

public class GuiSceneEditor extends SceneEditor {
    @Override
    public void init(IEditorSite site, IEditorInput input) throws PartInitException {
        super.init(site, input);
        SceneOutlinePage page = (SceneOutlinePage) getAdapter(IContentOutlinePage.class);
        page.setSupportsReordering(true);
    }
}

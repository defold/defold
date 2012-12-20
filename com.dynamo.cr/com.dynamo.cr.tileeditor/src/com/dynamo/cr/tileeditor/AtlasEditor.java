package com.dynamo.cr.tileeditor;

import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.part.IPageBookViewPage;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.sceneed.core.IModelListener;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.ui.SceneEditor;
import com.dynamo.cr.sceneed.ui.SceneOutlinePage;
import com.google.inject.AbstractModule;
import com.google.inject.Singleton;

public class AtlasEditor extends SceneEditor {

    private static Logger logger = LoggerFactory.getLogger(AtlasEditor.class);

    private IPageBookViewPage curveEditorPage;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISceneView.IPresenter.class).to(AtlasEditorPresenter.class).in(Singleton.class);
            bind(IModelListener.class).to(AtlasEditorPresenter.class).in(Singleton.class);
        }
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        return super.getAdapter(adapter);
    }

    @Override
    protected com.google.inject.Module createOverrideModule() {
        return new Module();
    }

    @Override
    public String getContextID() {
        return Activator.ATLAS_CONTEXT_ID;
    }

    @Override
    public void partOpened(IWorkbenchPart part) {
        super.partOpened(part);
    }

    @Override
    public void init(IEditorSite site, IEditorInput input) throws PartInitException {
        super.init(site, input);
        SceneOutlinePage page = (SceneOutlinePage)getAdapter(IContentOutlinePage.class);
        page.setSupportsReordering(true);
    }
}

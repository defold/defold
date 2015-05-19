package com.dynamo.cr.guied;

import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.ISelectionChangedListener;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.views.contentoutline.IContentOutlinePage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.LayoutNode;
import com.dynamo.cr.sceneed.ui.SceneEditor;
import com.dynamo.cr.sceneed.ui.SceneOutlinePage;

public class GuiSceneEditor extends SceneEditor implements ISelectionChangedListener {
    private static Logger logger = LoggerFactory.getLogger(GuiSceneEditor.class);

    @Override
    public void init(IEditorSite site, IEditorInput input) throws PartInitException {
        super.init(site, input);
        SceneOutlinePage page = (SceneOutlinePage) getAdapter(IContentOutlinePage.class);
        page.setSupportsReordering(true);
        this.sceneRenderViewProvider.addSelectionChangedListener(this);
    }

    @Override
    protected void handleProjectPropertiesChanged(ProjectProperties properties) {
        super.handleProjectPropertiesChanged(properties);
        getScenePresenter().onRefreshSceneView();
    }

    @Override
    protected void handleReloadResourceChanged(IResourceChangeEvent event) {
        super.handleReloadResourceChanged(event);
        try {
            event.getDelta().accept(new IResourceDeltaVisitor() {
                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    IResource resource = delta.getResource();
                    if(resource != null) {
                        String ext = resource.getFileExtension();
                        if (ext != null && ext.equals("display_profiles")) {
                            getScenePresenter().onRefreshSceneView();
                            return false;
                        }
                    }
                    return true;
                }
            });
        } catch (CoreException e) {
            logger.error("Error occurred while refreshing files", e);
        }
    }

    public void selectionChanged(SelectionChangedEvent event) {
        if (event.getSelection() instanceof IStructuredSelection) {
            Object[] selection = ((IStructuredSelection)event.getSelection()).toArray();
            for (Object object : selection) {
                // handle selection of Layoutnode (set as current layout)
                if (object instanceof LayoutNode) {
                    LayoutNode node = (LayoutNode) object;
                    GuiSceneNode scene = (GuiSceneNode) node.getModel().getRoot();
                    scene.setCurrentLayout(node.getId());
                    break;
                }
            }
        }
    }

    @Override
    public String getContextID() {
        return Activator.GUIEDITOR_CONTEXT_ID;
    }

}

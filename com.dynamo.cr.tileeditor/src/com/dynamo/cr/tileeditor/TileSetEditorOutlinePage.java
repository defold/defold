package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.util.List;

import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IEditorRegistry;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.views.contentoutline.ContentOutline;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.tileeditor.core.TileSetPresenter;

public class TileSetEditorOutlinePage extends ContentOutlinePage implements ISelectionListener {

    private final TileSetPresenter presenter;
    private final RootItem root;

    public TileSetEditorOutlinePage(TileSetPresenter presenter) {
        this.presenter = presenter;
        this.root = new RootItem();
    }

    @Override
    public void init(IPageSite pageSite) {
        super.init(pageSite);
        //        IActionBars actionBars = pageSite.getActionBars();
        //        String undoId = ActionFactory.UNDO.getId();
        //        String redoId = ActionFactory.REDO.getId();
        //        actionBars.setGlobalActionHandler(undoId, editor.getAction(undoId));
        //        actionBars.setGlobalActionHandler(redoId, editor.getAction(redoId));
    }

    @Override
    public void dispose() {
        super.dispose();
        getSite().getPage().removeSelectionListener(this);
    }

    public void setInput(List<String> collisionGroups, List<Color> collisionGroupColors) {
        this.root.collisionGroups.collisionGroups = collisionGroups.toArray(new String[collisionGroups.size()]);
        this.root.collisionGroups.collisionGroupColors = collisionGroupColors.toArray(new Color[collisionGroupColors.size()]);
        if (getTreeViewer() != null) {
            getTreeViewer().setInput(this.root);
            getTreeViewer().expandToLevel(2);
        }
    }

    static class RootItem {
        public CollisionGroupsItem collisionGroups;

        public RootItem() {
            this.collisionGroups = new CollisionGroupsItem();
        }
    }

    static class CollisionGroupsItem {
        public String[] collisionGroups;
        public Color[] collisionGroupColors;
    }

    class OutlineContentProvider implements ITreeContentProvider {
        @Override
        public void dispose() {
        }

        @Override
        public void inputChanged(Viewer viewer, Object oldInput, Object newInput) {
        }

        @Override
        public Object[] getElements(Object inputElement) {
            if (inputElement instanceof RootItem) {
                return new Object[] { ((RootItem)inputElement).collisionGroups };
            } else if (inputElement instanceof CollisionGroupsItem) {
                return ((CollisionGroupsItem)inputElement).collisionGroups;
            }
            return null;
        }

        @Override
        public Object[] getChildren(Object parentElement) {
            return getElements(parentElement);
        }

        @Override
        public Object getParent(Object element) {
            if (element instanceof RootItem) {
                return null;
            } else if (element instanceof CollisionGroupsItem) {
                return root;
            } else if (element instanceof String) {
                return root.collisionGroups;
            }
            return null;
        }

        @Override
        public boolean hasChildren(Object element) {
            if (element instanceof RootItem) {
                return true;
            } else if (element instanceof CollisionGroupsItem) {
                CollisionGroupsItem item = (CollisionGroupsItem)element;
                return item.collisionGroups != null && item.collisionGroups.length > 0;
            }
            return false;
        }
    }

    class OutlineColumnLabelProvider extends ColumnLabelProvider {

        private final IEditorRegistry registry;

        public OutlineColumnLabelProvider() {
            this.registry = PlatformUI.getWorkbench().getEditorRegistry();
        }

        @Override
        public Image getImage(Object element) {
            ISharedImages sharedImages = PlatformUI.getWorkbench().getSharedImages();
            Image image = null;

            if (element instanceof RootItem) {
                image = this.registry.getImageDescriptor(".tileset").createImage();
            } else if (element instanceof CollisionGroupsItem) {
                image = sharedImages.getImage(ISharedImages.IMG_OBJ_FOLDER);
            }

            if (image != null)
                return image;
            else
                return super.getImage(element);
        }

        @Override
        public String getText(Object element) {
            if (element instanceof RootItem) {
                return "Tile Set";
            } else if (element instanceof CollisionGroupsItem) {
                return "Collision Groups";
            } else if (element instanceof String) {
                return (String)element;
            } else {
                return super.getText(element);
            }
        }
    }

    @Override
    public void createControl(Composite parent) {
        super.createControl(parent);

        final TreeViewer viewer = getTreeViewer();
        ColumnViewerToolTipSupport.enableFor(viewer);
        viewer.getTree().setHeaderVisible(false);
        viewer.setContentProvider(new OutlineContentProvider());
        viewer.setLabelProvider(new LabelProvider());
        viewer.setLabelProvider(new OutlineColumnLabelProvider());
        viewer.setInput(this.root);
        viewer.expandToLevel(2);

        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext("com.dynamo.cr.tileseteditor.contexts.TileSetEditor");

        getSite().getPage().addSelectionListener(this);

        this.presenter.refresh();
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (! (part instanceof ContentOutline)) {
            TreeViewer viewer = getTreeViewer();
            viewer.setSelection(selection);
        }
    }

}

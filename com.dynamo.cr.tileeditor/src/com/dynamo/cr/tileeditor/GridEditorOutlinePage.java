package com.dynamo.cr.tileeditor;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;

import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.SelectionChangedEvent;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorRegistry;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.cr.tileeditor.core.Layer;

public class GridEditorOutlinePage extends ContentOutlinePage implements IGridEditorOutlinePage {

    private static final String MENU_ID = "com.dynamo.cr.tileeditor.menus.TileSetOutlineContext";

    private final IGridView.Presenter presenter;
    private final UndoActionHandler undoHandler;
    private final RedoActionHandler redoHandler;
    private final RootItem root;
    private final Image gridImage;
    private final Image layerImage;
    // needed to avoid circumference when selecting programatically vs manually
    private boolean ignoreSelection = false;

    @Inject
    public GridEditorOutlinePage(IGridView.Presenter presenter, UndoActionHandler undoHandler, RedoActionHandler redoHandler) {
        this.presenter = presenter;
        this.undoHandler = undoHandler;
        this.redoHandler = redoHandler;
        this.root = new RootItem();

        ImageRegistry imageRegist = Activator.getDefault()
                .getImageRegistry();
        this.gridImage = imageRegist
                .getDescriptor(Activator.GRID_IMAGE_ID).createImage();
        this.layerImage = imageRegist
                .getDescriptor(Activator.LAYER_IMAGE_ID).createImage();
    }

    @Override
    public void init(IPageSite pageSite) {
        super.init(pageSite);
        IActionBars actionBars = pageSite.getActionBars();
        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), undoHandler);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), redoHandler);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.tileeditor.IGridEditorOutlinePage#setInput(java.util.List, int)
     */
    @Override
    public void setInput(List<Layer> layers, int selectedLayer) {
        int n = layers.size();
        LayersItem layersItem = this.root.grid.layers;
        if (layersItem.items == null || layersItem.items.length != n) {
            layersItem.items = new Layer[n];
        }
        layers.toArray(layersItem.items);
        TreeViewer viewer = getTreeViewer();
        if (viewer != null) {
            this.ignoreSelection = true;
            viewer.setInput(this.root);
            viewer.expandToLevel(2);
            if (selectedLayer >= 0) {
                List<Layer> selectedItems = new ArrayList<Layer>(1);
                selectedItems.add(this.root.grid.layers.items[selectedLayer]);
                viewer.setSelection(new StructuredSelection(selectedItems));
            } else {
                List<GridItem> selectedItems = new ArrayList<GridItem>(1);
                selectedItems.add(this.root.grid);
                viewer.setSelection(new StructuredSelection(selectedItems));
            }
            ignoreSelection = false;
        }
    }

    private static class RootItem {
        public GridItem grid;

        public RootItem() {
            this.grid = new GridItem();
        }
    }

    private static class GridItem {
        public LayersItem layers;

        public GridItem() {
            this.layers = new LayersItem();
        }
    }

    private static class LayersItem {
        public Layer[] items;
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
                return new Object[] { ((RootItem)inputElement).grid };
            } else if (inputElement instanceof GridItem) {
                return new Object[] { ((GridItem)inputElement).layers };
            } else if (inputElement instanceof LayersItem) {
                return ((LayersItem)inputElement).items;
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
            } else if (element instanceof GridItem) {
                return root;
            } else if (element instanceof LayersItem) {
                return root.grid;
            } else if (element instanceof Layer) {
                return root.grid.layers;
            }
            return null;
        }

        @Override
        public boolean hasChildren(Object element) {
            if (element instanceof RootItem || element instanceof GridItem) {
                return true;
            } else if (element instanceof LayersItem) {
                LayersItem item = (LayersItem)element;
                return item.items != null && item.items.length > 0;
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

            if (element instanceof RootItem || element instanceof GridItem) {
                image = gridImage;
            } else if (element instanceof LayersItem) {
                image = sharedImages.getImage(ISharedImages.IMG_OBJ_FOLDER);
            } else if (element instanceof Layer) {
                image = layerImage;
            }

            if (image != null)
                return image;
            else
                return super.getImage(element);
        }

        @Override
        public String getText(Object element) {
            if (element instanceof RootItem || element instanceof GridItem) {
                return "Grid";
            } else if (element instanceof LayersItem) {
                return "Layers";
            } else if (element instanceof Layer) {
                return ((Layer)element).getId();
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

        // Pop-up context menu
        MenuManager menuManager = new MenuManager();
        menuManager.add(new Separator(IWorkbenchActionConstants.MB_ADDITIONS));
        Menu menu = menuManager.createContextMenu(viewer.getTree());
        viewer.getTree().setMenu(menu);
        getSite().registerContextMenu(MENU_ID, menuManager, viewer);

        // This makes sure the context will be active while this component is
        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(Activator.GRID_CONTEXT_ID);

        this.presenter.onRefresh();
    }

    @Override
    public void selectionChanged(SelectionChangedEvent event) {
        super.selectionChanged(event);
        if (!this.ignoreSelection) {
            if (event.getSelection() instanceof IStructuredSelection) {
                List<String> selectedItems = new ArrayList<String>();
                Object[] selection = ((IStructuredSelection)event.getSelection()).toArray();
                for (Object object : selection) {
                    if (object instanceof Layer) {
                        Layer item = (Layer)object;
                        selectedItems.add(item.getId());
                    }
                }
                // TODO: Perform selection
                //                String[] selectedCollisionGroups = selectedItems.toArray(new String[selectedItems.size()]);
                //                this.presenter.selectCollisionGroups(selectedCollisionGroups);
            }
        }
    }

    @Override
    public void refresh() {
        if (getTreeViewer() != null) {
            getTreeViewer().refresh();
        }
    }

}

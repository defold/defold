package com.dynamo.cr.sceneed;

import javax.inject.Inject;

import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.SWT;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.sceneed.core.INodeView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodeManager;

public class NodeOutlinePage extends ContentOutlinePage implements INodeOutlinePage, ISelectionListener {

    private static final String MENU_ID = "com.dynamo.cr.sceneed.menus.NodeOutlineContext";

    private final NodeManager manager;
    private final UndoActionHandler undoHandler;
    private final RedoActionHandler redoHandler;
    private final RootItem root;

    @Inject
    public NodeOutlinePage(NodeManager manager, UndoActionHandler undoHandler, RedoActionHandler redoHandler) {
        this.manager = manager;
        this.undoHandler = undoHandler;
        this.redoHandler = redoHandler;
        this.root = new RootItem();
    }

    @Override
    public void init(IPageSite pageSite) {
        super.init(pageSite);
        IActionBars actionBars = pageSite.getActionBars();
        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), undoHandler);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), redoHandler);
    }

    @Override
    public void dispose() {
        super.dispose();
        getSite().getPage().removeSelectionListener(this);
    }

    @Override
    public void setInput(Node node) {
        TreeViewer viewer = getTreeViewer();
        if (viewer != null) {
            this.root.node = node;
            viewer.setInput(this.root);
            if (node != null) {
                viewer.setSelection(new StructuredSelection(node));
            }
            viewer.expandToLevel(2);
        }
    }

    @Override
    public void update(Node node) {
        TreeViewer viewer = getTreeViewer();
        if (viewer != null) {
            viewer.refresh(node);
        }
    }

    private static class RootItem {
        public Node node;
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
            if (inputElement instanceof RootItem && root.node != null) {
                return new Object[] {root.node};
            } else if (inputElement instanceof Node) {
                Node node = (Node)inputElement;
                return node.getChildren().toArray();
            }

            return new Object[0];
        }

        @Override
        public Object[] getChildren(Object parentElement) {
            return getElements(parentElement);
        }

        @Override
        public Object getParent(Object element) {
            if (element instanceof Node) {
                Node node = (Node)element;
                Node parent = node.getParent();
                if (parent != null) {
                    return parent;
                } else {
                    return root;
                }
            }
            return null;
        }

        @Override
        public boolean hasChildren(Object element) {
            if (element instanceof RootItem) {
                return root.node != null;
            } else if (element instanceof Node) {
                Node node = (Node)element;
                return node.hasChildren();
            }
            return false;
        }
    }

    class OutlineColumnLabelProvider extends ColumnLabelProvider {

        @Override
        public Image getImage(Object element) {
            Image image = null;

            if (element instanceof Node) {
                Node node = (Node)element;
                image = node.getImage();
            }

            if (image != null)
                return image;
            else
                return super.getImage(element);
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
        contextService.activateContext(Activator.SCENEED_CONTEXT_ID);

        getSite().getPage().addSelectionListener(this);

        INodeView.Presenter presenter = manager.getDefaultPresenter();
        presenter.onRefresh();
    }

    @Override
    protected int getTreeStyle() {
        return SWT.SINGLE | SWT.H_SCROLL | SWT.V_SCROLL;
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (part instanceof NodeEditor) {
            TreeViewer viewer = getTreeViewer();
            viewer.setSelection(selection, true);
        }
    }

}

package com.dynamo.cr.sceneed.ui;

import javax.inject.Inject;

import org.eclipse.core.commands.NotHandledException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.DecorationContext;
import org.eclipse.jface.viewers.DelegatingStyledCellLabelProvider.IStyledLabelProvider;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.StyledString;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerCell;
import org.eclipse.swt.SWT;
import org.eclipse.swt.events.MouseAdapter;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.handlers.IHandlerService;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.editor.core.ILogger;
import com.dynamo.cr.editor.ui.DecoratingDefoldLabelProvider;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;

public class SceneOutlinePage extends ContentOutlinePage implements ISceneOutlinePage, ISelectionListener {

    private static final String MENU_ID = "com.dynamo.cr.sceneed.menus.sceneOutlineContext";

    private final ISceneView.IPresenter presenter;
    private final UndoActionHandler undoHandler;
    private final RedoActionHandler redoHandler;
    private final RootItem root;
    private final ILogger logger;

    @Inject
    public SceneOutlinePage(ISceneView.IPresenter presenter, UndoActionHandler undoHandler, RedoActionHandler redoHandler, ILogger logger) {
        this.presenter = presenter;
        this.undoHandler = undoHandler;
        this.redoHandler = redoHandler;
        this.root = new RootItem();
        this.logger = logger;
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
        if (viewer != null && !viewer.getTree().isDisposed()) {
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

    class OutlineLabelProvider extends ColumnLabelProvider implements IStyledLabelProvider {

        @Override
        public Image getImage(Object element) {
            Image image = null;

            if (element instanceof Node) {
                Node node = (Node)element;
                image = node.getIcon();
            }

            if (image != null)
                return image;
            else
                return super.getImage(element);
        }

        private String getMostSevereStatusText(IStatus status) {
            if (status.getSeverity() != IStatus.OK) {
                String text = status.getMessage();
                IStatus[] children = status.getChildren();
                if (children.length > 0) {
                    for (int i = 0; i < children.length; ++i) {
                        if (children[i].getSeverity() == status.getSeverity()) {
                            return getMostSevereStatusText(children[i]);
                        }
                    }
                }
                return text;
            }
            return null;
        }

        @Override
        public String getToolTipText(Object element) {
            if (element instanceof Node) {
                Node node = (Node)element;
                IStatus status = node.validate();
                return getMostSevereStatusText(status);
            }
            return super.getToolTipText(element);
        }

        @Override
        public Image getToolTipImage(Object object) {
            if (object instanceof Node) {
                Node node = (Node)object;
                IStatus status = node.validate();
                switch (status.getSeverity()) {
                case IStatus.INFO:
                    return Activator.getDefault().getImageRegistry().get(Activator.IMG_OVERLAY_INFO);
                case IStatus.WARNING:
                    return Activator.getDefault().getImageRegistry().get(Activator.IMG_OVERLAY_WARNING);
                case IStatus.ERROR:
                    return Activator.getDefault().getImageRegistry().get(Activator.IMG_OVERLAY_ERROR);
                }
            }
            return super.getToolTipImage(object);
        }

        @Override
        public boolean isLabelProperty(Object element, String property) {
            return true;
        }

        @Override
        public StyledString getStyledText(Object element) {
            return new StyledString(getText(element));
        }

    }

    @Override
    public void createControl(Composite parent) {
        super.createControl(parent);

        final TreeViewer viewer = getTreeViewer();
        ColumnViewerToolTipSupport.enableFor(viewer);
        viewer.getTree().setHeaderVisible(false);
        viewer.setContentProvider(new OutlineContentProvider());
        viewer.setLabelProvider(new DecoratingDefoldLabelProvider(new OutlineLabelProvider(), Activator.getDefault().getWorkbench().getDecoratorManager().getLabelDecorator(), DecorationContext.DEFAULT_CONTEXT));
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

        viewer.getTree().addMouseListener(new MouseAdapter() {
            @Override
            public void mouseDoubleClick(MouseEvent event) {
                ViewerCell cell = viewer.getCell(new Point(event.x, event.y));
                if (cell == null)
                    return;

                IHandlerService handlerService = (IHandlerService)getSite().getService(IHandlerService.class);
                try {
                    Event e = new Event();
                    e.widget = viewer.getTree();
                    handlerService.executeCommand(Activator.ENTER_COMMAND_ID, e);
                } catch (NotHandledException e) {
                    // Completely fine
                } catch (Throwable e) {
                    logger.logException(e);
                }
            }
        });

        this.presenter.onRefresh();
    }

    @Override
    protected int getTreeStyle() {
        return SWT.SINGLE | SWT.H_SCROLL | SWT.V_SCROLL;
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (part instanceof SceneEditor) {
            TreeViewer viewer = getTreeViewer();
            viewer.setSelection(selection, true);
        }
    }

}

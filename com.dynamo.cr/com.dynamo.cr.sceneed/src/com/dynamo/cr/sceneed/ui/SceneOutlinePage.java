package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.inject.Inject;

import org.eclipse.core.commands.NotHandledException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.jface.action.MenuManager;
import org.eclipse.jface.action.Separator;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.DecorationContext;
import org.eclipse.jface.viewers.DelegatingStyledCellLabelProvider.IStyledLabelProvider;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.viewers.StyledString;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.jface.viewers.ViewerCell;
import org.eclipse.jface.viewers.StyledString.*;
import org.eclipse.swt.SWT;
import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.events.MouseAdapter;
import org.eclipse.swt.events.MouseEvent;
import org.eclipse.swt.graphics.Font;
import org.eclipse.swt.graphics.FontData;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.Point;
import org.eclipse.swt.graphics.TextStyle;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.Menu;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IDecoratorManager;
import org.eclipse.ui.IWorkbenchActionConstants;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.handlers.IHandlerService;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.ui.DecoratingDefoldLabelProvider;
import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.util.NodeListTransfer;
import com.dynamo.cr.sceneed.ui.decorators.SceneDecorator;
import com.dynamo.cr.sceneed.ui.util.NodeListDNDListener;

public class SceneOutlinePage extends ContentOutlinePage implements ISceneOutlinePage {

    private static Logger logger = LoggerFactory.getLogger(SceneOutlinePage.class);
    private static final String MENU_ID = "com.dynamo.cr.sceneed.menus.sceneOutlineContext";

    private final SceneEditor scenedEditor;
    private final ISceneView.IPresenter presenter;
    private final ISceneView.IPresenterContext presenterContext;
    private final UndoActionHandler undoHandler;
    private final RedoActionHandler redoHandler;
    private final RootItem root;
    private final OutlineLabelProvider labelProvider;
    private boolean supportsReordering = false;
    private NodeListDNDListener dndListener;

    @Inject
    public SceneOutlinePage(SceneEditor sceneEditor, ISceneView.IPresenter presenter, ISceneView.IPresenterContext presenterContext, UndoActionHandler undoHandler, RedoActionHandler redoHandler) {
        this.scenedEditor = sceneEditor;
        this.presenter = presenter;
        this.presenterContext = presenterContext;
        this.undoHandler = undoHandler;
        this.redoHandler = redoHandler;
        this.root = new RootItem();
        this.labelProvider = new OutlineLabelProvider();
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
    public void refresh() {
        TreeViewer viewer = getTreeViewer();
        if (viewer != null && !viewer.getTree().isDisposed()) {
            viewer.refresh(true);
            Display.getDefault().asyncExec(new Runnable() {
                @Override
                public void run() {
                    IDecoratorManager idm = PlatformUI.getWorkbench().getDecoratorManager();
                    idm.update(SceneDecorator.DECORATOR_ID);
                }
            });
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
                if (node.isEditable()) {
                    List<Node> children = node.getChildren();
                    List<Node> editables = new ArrayList<Node>(children.size());
                    for (Node child : children) {
                        if (child.isEditable()) {
                            editables.add(child);
                        }
                    }
                    return editables.toArray();
                }
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
                return node.isEditable() && node.hasChildren() && getElements(element).length > 0;
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
                IStatus status = node.getStatus();
                return getMostSevereStatusText(status);
            }
            return super.getToolTipText(element);
        }

        @Override
        public Image getToolTipImage(Object object) {
            if (object instanceof Node) {
                Node node = (Node)object;
                IStatus status = node.getStatus();
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

        private StyledString.Styler createStyledTextStyler(final String fontName, final int fontHeight, final int fontStyle){
            Styler result = new StyledString.Styler(){
                @Override public void applyStyles(TextStyle textstyle){
                  textstyle.font = new Font(null, fontName, fontHeight, fontStyle);
                }
            };
            return result;
        }

        @Override
        public StyledString getStyledText(Object element) {
            if (element instanceof Node) {
                Node node = (Node)element;
                if (!node.isEditable()) {
                    // TODO not the best way to use the qualifier style, but much more work to roll a custom
                    // From javadoc:
                    // "Identifier for the color used to show extra informations in labels, as a qualified name.
                    // For example in 'Foo.txt - myproject/bar', the qualifier is '- myproject/bar'."
                    return new StyledString(getText(node), StyledString.QUALIFIER_STYLER);
                }
                int style = node.getLabelTextStyle();
                if(style != SWT.NORMAL) {
                    TreeViewer viewer = getTreeViewer();
                    FontData fontData = viewer.getControl().getDisplay().getSystemFont().getFontData()[0];
                    return new StyledString(getText(node), createStyledTextStyler(fontData.getName(), fontData.getHeight(), fontData.getStyle() | style));
                }
            }
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
        viewer.setLabelProvider(new DecoratingDefoldLabelProvider(this.labelProvider, Activator.getDefault().getWorkbench().getDecoratorManager().getLabelDecorator(), DecorationContext.DEFAULT_CONTEXT));
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
        contextService.activateContext(scenedEditor.getContextID());

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
                    logger.error("Unexpected error occurred", e);
                }
            }
        });

        int operations = DND.DROP_MOVE | DND.DROP_COPY;
        Transfer[] transferTypes = new Transfer[] {NodeListTransfer.getInstance()};
        this.dndListener = new NodeListDNDListener(viewer, this.presenter, this.presenterContext, supportsReordering);
        viewer.addDragSupport(operations, transferTypes, dndListener);
        viewer.addDropSupport(operations, transferTypes, dndListener);

        this.presenter.onRefresh();
    }

    @Override
    protected int getTreeStyle() {
        return SWT.MULTI | SWT.H_SCROLL | SWT.V_SCROLL;
    }

    public void setSupportsReordering(boolean supportsReordering) {
        this.supportsReordering = supportsReordering;
        if (this.dndListener != null) {
            this.dndListener.setSupportsReordering(supportsReordering);
        }
    }
}

package com.dynamo.cr.guieditor;

import java.util.Collection;

import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.part.IPageSite;
import org.eclipse.ui.views.contentoutline.ContentOutline;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.guieditor.scene.BoxGuiNode;
import com.dynamo.cr.guieditor.scene.EditorFontDesc;
import com.dynamo.cr.guieditor.scene.EditorTextureDesc;
import com.dynamo.cr.guieditor.scene.GuiNode;
import com.dynamo.cr.guieditor.scene.GuiScene;
import com.dynamo.cr.guieditor.scene.TextGuiNode;

public class GuiEditorOutlinePage extends ContentOutlinePage implements ISelectionListener {

    private IGuiEditor editor;
    private Root root;

    public GuiEditorOutlinePage(IGuiEditor editor) {
        this.editor = editor;
    }

    @Override
    public void init(IPageSite pageSite) {
        super.init(pageSite);
        IActionBars actionBars = pageSite.getActionBars();
        String undoId = ActionFactory.UNDO.getId();
        String redoId = ActionFactory.REDO.getId();
        actionBars.setGlobalActionHandler(undoId, editor.getAction(undoId));
        actionBars.setGlobalActionHandler(redoId, editor.getAction(redoId));
    }

    @Override
    public void dispose() {
        super.dispose();
        getSite().getPage().removeSelectionListener(this);
    }

    static class Root {
        private GuiScene scene;

        public Root(GuiScene scene) {
            this.scene = scene;
        }
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
            if (inputElement instanceof Root) {
                return new Object[] { ((Root) inputElement).scene };
            } else if (inputElement instanceof GuiScene) {
                return new String[] { "Fonts", "Textures", "Nodes" };
            } else if (inputElement.equals("Fonts")) {
                Collection<EditorFontDesc> fonts = editor.getScene().getFonts();
                return fonts.toArray(new EditorFontDesc[fonts.size()]);
            } else if (inputElement.equals("Textures")) {
                Collection<EditorTextureDesc> textures = editor.getScene()
                        .getTextures();
                return textures.toArray(new EditorTextureDesc[textures.size()]);
            } else if (inputElement.equals("Nodes")) {
                GuiNode[] nodes = editor.getScene().getNodes();
                return nodes;
            }
            return null;
        }

        @Override
        public Object[] getChildren(Object parentElement) {
            return getElements(parentElement);
        }

        @Override
        public Object getParent(Object element) {
            if (element instanceof Root) {
                return null;
            } else if (element instanceof GuiScene) {
                return root;
            } else if (element instanceof EditorTextureDesc) {
                return "Textures";
            } else if (element instanceof EditorFontDesc) {
                return "Fonts";
            } else if (element instanceof GuiNode) {
                return "Nodes";
            } else if (element.equals("Fonts")) {
                return editor.getScene();
            } else if (element.equals("Textures")) {
                return editor.getScene();
            } else if (element.equals("Nodes")) {
                return editor.getScene();
            }
            return null;
        }

        @Override
        public boolean hasChildren(Object element) {
            return (element instanceof Root) || (element instanceof GuiScene)
                    || element.equals("Fonts") || element.equals("Textures")
                    || element.equals("Nodes");
        }
    }

    class OutlineColumnLabelProvider extends ColumnLabelProvider {

        private Image boxNodeImage;
        private Image textNodeImage;

        public OutlineColumnLabelProvider() {
            ImageRegistry imageRegist = Activator.getDefault()
                    .getImageRegistry();
            boxNodeImage = imageRegist.get(Activator.BOXNODE_IMAGE_ID);
            textNodeImage = imageRegist.get(Activator.TEXTNODE_IMAGE_ID);
        }

        Image getImageFromFilename(String name) {
            if (name.lastIndexOf('.') != -1) {
                return Activator.getDefault().getImage(name);
            }
            return null;
        }

        @Override
        public Image getImage(Object element) {
            ISharedImages sharedImages = PlatformUI.getWorkbench().getSharedImages();
            Image image = null;

            if (element instanceof GuiScene) {
                image = Activator.getDefault().getImage("gui");
            } else if (element.equals("Fonts")) {
                image = sharedImages.getImage(ISharedImages.IMG_OBJ_FOLDER);
            } else if (element.equals("Textures")) {
                image = sharedImages.getImage(ISharedImages.IMG_OBJ_FOLDER);
            } else if (element.equals("Nodes")) {
                image = sharedImages.getImage(ISharedImages.IMG_OBJ_FOLDER);
            } else if (element instanceof EditorFontDesc) {
                EditorFontDesc fontDesc = (EditorFontDesc) element;
                String name = fontDesc.getFont();
                image = getImageFromFilename(name);
            } else if (element instanceof EditorTextureDesc) {
                EditorTextureDesc textureDesc = (EditorTextureDesc) element;
                String name = textureDesc.getTexture();
                image = getImageFromFilename(name);
            } else if (element instanceof BoxGuiNode) {
                image = boxNodeImage;
            } else if (element instanceof TextGuiNode) {
                image = textNodeImage;
            }

            if (image != null)
                return image;
            else
                return super.getImage(element);
        }

        @Override
        public String getText(Object element) {
            if (element instanceof GuiScene) {
                return "Scene";
            } else if (element instanceof EditorTextureDesc) {
                EditorTextureDesc textureDesc = (EditorTextureDesc) element;
                return String.format("%s [%s]", textureDesc.getName(),
                        textureDesc.getTexture());
            } else if (element instanceof EditorFontDesc) {
                EditorFontDesc fontDesc = (EditorFontDesc) element;
                return String.format("%s [%s]", fontDesc.getName(),
                        fontDesc.getFont());
            } else if (element instanceof GuiNode) {
                GuiNode node = (GuiNode) element;
                String id = node.getId();
                if (id.length() > 0)
                    return id;
                else
                    return "<NO ID>";
            } else {
                return super.getText(element);
            }
        }
    }

    @Override
    public void createControl(Composite parent) {
        super.createControl(parent);

        root = new Root(editor.getScene());
        final TreeViewer viewer = getTreeViewer();
        ColumnViewerToolTipSupport.enableFor(viewer);
        viewer.getTree().setHeaderVisible(false);
        viewer.setContentProvider(new OutlineContentProvider());
        viewer.setLabelProvider(new LabelProvider());
        viewer.setLabelProvider(new OutlineColumnLabelProvider());
        viewer.setInput(root);
        viewer.expandToLevel(3);

        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService
                .activateContext("com.dynamo.cr.guieditor.contexts.GuiEditor");

        getSite().getPage().addSelectionListener(this);
    }

    public void refresh() {
        getTreeViewer().refresh();
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (! (part instanceof ContentOutline)) {
            TreeViewer viewer = getTreeViewer();
            viewer.setSelection(selection);
        }
    }

}

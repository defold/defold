package com.dynamo.cr.tileeditor;

import java.awt.Color;
import java.util.List;

import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.jface.viewers.ColumnLabelProvider;
import org.eclipse.jface.viewers.ColumnViewerToolTipSupport;
import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.ITreeContentProvider;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.TreeViewer;
import org.eclipse.jface.viewers.Viewer;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IEditorRegistry;
import org.eclipse.ui.ISelectionListener;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.views.contentoutline.ContentOutline;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.tileeditor.core.TileSetPresenter;

public class TileSetEditorOutlinePage extends ContentOutlinePage implements ISelectionListener {

    private final TileSetPresenter presenter;
    private final RootItem root;
    private final Image collisionGroupImage;

    public TileSetEditorOutlinePage(TileSetPresenter presenter) {
        this.presenter = presenter;
        this.root = new RootItem();

        ImageRegistry imageRegist = Activator.getDefault()
                .getImageRegistry();
        this.collisionGroupImage = imageRegist
                .getDescriptor(Activator.COLLISION_GROUP_IMAGE_ID).createImage();
    }

    @Override
    public void dispose() {
        super.dispose();
        getSite().getPage().removeSelectionListener(this);
        if (this.root.collisionGroups.items != null) {
            for (CollisionGroupItem item : this.root.collisionGroups.items) {
                item.image.dispose();
            }
        }
    }

    public void setInput(List<String> collisionGroups, List<Color> collisionGroupColors) {
        int n = collisionGroups.size();
        CollisionGroupItem[] items = new CollisionGroupItem[n];
        for (int i = 0; i < n; ++i) {
            items[i] = new CollisionGroupItem();
            items[i].group = collisionGroups.get(i);
        }
        TreeViewer viewer = getTreeViewer();
        if (viewer != null) {
            updateCollisionGroupImages(items, collisionGroupColors.toArray(new Color[collisionGroupColors.size()]));
        }
        CollisionGroupItem[] oldItems = this.root.collisionGroups.items;
        this.root.collisionGroups.items = items;
        if (oldItems != null) {
            for (CollisionGroupItem item : oldItems) {
                if (item.image != null) {
                    item.image.dispose();
                }
            }
        }
        if (viewer != null) {
            viewer.setInput(this.root);
            viewer.expandToLevel(2);
        }
    }

    public static class RootItem {
        public CollisionGroupsItem collisionGroups;

        public RootItem() {
            this.collisionGroups = new CollisionGroupsItem();
        }
    }

    public static class CollisionGroupsItem {
        public CollisionGroupItem[] items;
    }

    public static class CollisionGroupItem
    {
        public String group;
        public Image image;
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
                return ((CollisionGroupsItem)inputElement).items;
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
            } else if (element instanceof CollisionGroupItem) {
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

            if (element instanceof RootItem) {
                image = this.registry.getImageDescriptor(".tileset").createImage();
            } else if (element instanceof CollisionGroupsItem) {
                image = sharedImages.getImage(ISharedImages.IMG_OBJ_FOLDER);
            } else if (element instanceof CollisionGroupItem) {
                CollisionGroupItem item = (CollisionGroupItem)element;
                if (item.image != null) {
                    return item.image;
                } else {
                    return collisionGroupImage;
                }
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
            } else if (element instanceof CollisionGroupItem) {
                return ((CollisionGroupItem)element).group;
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

        getSite().getPage().addSelectionListener(this);

        // This makes sure the context will be active while this component is
        IContextService contextService = (IContextService) getSite()
                .getService(IContextService.class);
        contextService.activateContext(Activator.CONTEXT_ID);

        this.presenter.refresh();
    }

    @Override
    public void selectionChanged(IWorkbenchPart part, ISelection selection) {
        if (! (part instanceof ContentOutline)) {
            TreeViewer viewer = getTreeViewer();
            viewer.setSelection(selection);
        }
    }

    private void updateCollisionGroupImages(CollisionGroupItem[] items, Color[] colors) {
        int n = items.length;
        float f = 1.0f / 255.0f;
        for (int i = 0; i < n; ++i) {
            ImageData data = this.collisionGroupImage.getImageData();
            Color color = colors[i];
            float s_r = color.getRed() * f;
            float s_g = color.getGreen() * f;
            float s_b = color.getBlue() * f;
            float s_a = color.getAlpha() * f;
            int width = data.width;
            int height = data.height;
            for(int y = 0; y < height; ++y) {
                for(int x = 0; x < width; ++x) {
                    int p = x + y * width;
                    float t_r = (data.data[p * 3 + 0] & 255) * f;
                    float t_g = (data.data[p * 3 + 1] & 255) * f;
                    float t_b = (data.data[p * 3 + 2] & 255) * f;
                    t_r = t_r * (1.0f - s_a) + s_r * s_a;
                    t_g = t_g * (1.0f - s_a) + s_g * s_a;
                    t_b = t_b * (1.0f - s_a) + s_b * s_a;
                    data.data[p * 3 + 0] = (byte)(t_r * 255.0f);
                    data.data[p * 3 + 1] = (byte)(t_g * 255.0f);
                    data.data[p * 3 + 2] = (byte)(t_b * 255.0f);
                }
            }
            items[i].image = new Image(getSite().getShell().getDisplay(), data);
        }
    }

}

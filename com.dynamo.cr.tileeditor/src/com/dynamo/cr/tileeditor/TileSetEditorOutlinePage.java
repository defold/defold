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
import org.eclipse.ui.views.contentoutline.ContentOutline;
import org.eclipse.ui.views.contentoutline.ContentOutlinePage;

import com.dynamo.cr.tileeditor.core.TileSetPresenter;

public class TileSetEditorOutlinePage extends ContentOutlinePage implements ISelectionListener {

    private final TileSetPresenter presenter;
    private final RootItem root;
    private final Image collisionGroupImage;
    private Image[] collisionGroupImages;

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
        for (Image image : this.collisionGroupImages) {
            image.dispose();
        }
    }

    public void setInput(List<String> collisionGroups, List<Color> collisionGroupColors) {
        this.root.collisionGroups.collisionGroups = collisionGroups.toArray(new String[collisionGroups.size()]);
        this.root.collisionGroups.collisionGroupColors = collisionGroupColors.toArray(new Color[collisionGroupColors.size()]);
        TreeViewer viewer = getTreeViewer();
        if (viewer != null) {
            updateCollisionGroupImages();
            viewer.setInput(this.root);
            viewer.expandToLevel(2);
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
            } else if (element instanceof String) {
                int n = root.collisionGroups.collisionGroups.length;
                if (collisionGroupImages != null && collisionGroupImages.length == n) {
                    for (int i = 0; i < n; ++i) {
                        if (root.collisionGroups.collisionGroups[i].equals(element)) {
                            image = collisionGroupImages[i];
                            break;
                        }
                    }
                } else {
                    image = collisionGroupImage;
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

    private void updateCollisionGroupImages() {
        int n = this.root.collisionGroups.collisionGroups.length;
        Color[] colors = this.root.collisionGroups.collisionGroupColors;
        Image[] images = new Image[n];
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
            images[i] = new Image(getSite().getShell().getDisplay(), data);
        }
        Image[] oldImages = this.collisionGroupImages;
        this.collisionGroupImages = images;
        if (oldImages != null) {
            for (Image image : oldImages) {
                image.dispose();
            }
        }
    }

}

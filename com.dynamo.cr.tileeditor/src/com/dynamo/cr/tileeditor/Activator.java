
package com.dynamo.cr.tileeditor;

import java.awt.Color;

import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.PaletteData;
import org.eclipse.swt.graphics.RGB;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.ui.ColorUtil;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.tileeditor"; //$NON-NLS-1$

    // Context IDs
    public static final String TILE_SET_CONTEXT_ID = "com.dynamo.cr.tileeditor.contexts.TileSetEditor"; //$NON-NLS-1$
    public static final String GRID_CONTEXT_ID = "com.dynamo.cr.tileeditor.contexts.GridEditor"; //$NON-NLS-1$

    // Image ids
    public static final String COLLISION_GROUP_IMAGE_ID = "COLLISION_GROUP"; //$NON-NLS-1$
    public static final String GRID_IMAGE_ID = "GRID"; //$NON-NLS-1$
    public static final String LAYER_IMAGE_ID = "LAYER"; //$NON-NLS-1$

    // The shared instance
    private static Activator plugin;

    private final CollisionGroupCache collisionGroupCache;
    private final Color[] collisionGroupColors;

    /**
     * The constructor
     */
    public Activator() {
        this.collisionGroupCache = new CollisionGroupCache();
        int n = CollisionGroupCache.SIZE;
        this.collisionGroupColors = new Color[n];
        generateColors();
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#start(org.osgi.framework.BundleContext)
     */
    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        plugin = this;
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#stop(org.osgi.framework.BundleContext)
     */
    @Override
    public void stop(BundleContext context) throws Exception {
        plugin = null;
        super.stop(context);
    }

    /**
     * Returns the shared instance
     *
     * @return the shared instance
     */
    public static Activator getDefault() {
        return plugin;
    }

    public void addCollisionGroup(String group) {
        this.collisionGroupCache.add(group);
    }

    public void removeCollisionGroup(String group) {
        this.collisionGroupCache.remove(group);
    }

    public Image getCollisionGroupIcon(String group) {
        int index = this.collisionGroupCache.get(group);
        if (index >= 0) {
            String imageKey = COLLISION_GROUP_IMAGE_ID + index;
            Image image = getImageRegistry().get(imageKey);
            if (image == null) {
                generateCollisionImages();
                image = getImageRegistry().get(imageKey);
            }
            return image;
        }
        return null;
    }

    public Color getCollisionGroupColor(String group) {
        if (group != null && !group.isEmpty()) {
            int index = this.collisionGroupCache.get(group);
            if (index >= 0) {
                return this.collisionGroupColors[index];
            }
        }
        return new Color(1.0f, 1.0f, 1.0f, 1.0f);
    }

    @Override
    protected void initializeImageRegistry(ImageRegistry registry) {
        super.initializeImageRegistry(registry);

        registry.put(COLLISION_GROUP_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/collision_group.png"));
        registry.put(GRID_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/tile_grid.png"));
        registry.put(LAYER_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/layer.png"));
    }

    private void generateColors() {
        int index = 0;
        Color[] c = this.collisionGroupColors;
        c[index++] = ColorUtil.createColorFromHueAlpha(0.0f, 0.7f);
        generateColor(c, 0.5f, index);
    }

    private static void generateColor(Color[] c, float fraction, int index) {
        for (float f = fraction; index < CollisionGroupCache.SIZE && f < 1.0f; f += 2.0f * fraction) {
            c[index++] = ColorUtil.createColorFromHueAlpha(f * 360.0f, 0.7f);
        }
        if (index < CollisionGroupCache.SIZE) {
            generateColor(c, fraction * 0.5f, index);
        }
    }

    private void generateCollisionImages() {
        int n = CollisionGroupCache.SIZE;
        float f = 1.0f / 255.0f;
        Image baseImage = getImageRegistry().get(COLLISION_GROUP_IMAGE_ID);
        for (int i = 0; i < n; ++i) {
            ImageData data = baseImage.getImageData();
            Color color = this.collisionGroupColors[i];
            float s_r = color.getRed() * f;
            float s_g = color.getGreen() * f;
            float s_b = color.getBlue() * f;
            float s_a = color.getAlpha() * f;
            int imgSize = data.width * data.height;
            int[] pixels = new int[imgSize];
            data.getPixels(0, 0, imgSize, pixels, 0);
            PaletteData palette = data.palette;
            for (int p = 0; p < imgSize; ++p) {
                RGB rgb = palette.getRGB(pixels[p]);
                float t_r = rgb.red * f;
                float t_g = rgb.green * f;
                float t_b = rgb.blue * f;
                t_r = t_r * (1.0f - s_a) + s_r * s_a;
                t_g = t_g * (1.0f - s_a) + s_g * s_a;
                t_b = t_b * (1.0f - s_a) + s_b * s_a;
                rgb.red = (int) (t_r * 255.0f);
                rgb.green = (int) (t_g * 255.0f);
                rgb.blue = (int) (t_b * 255.0f);
                pixels[p] = palette.getPixel(rgb);
            }
            data.setPixels(0, 0, imgSize, pixels, 0);
            Image image = new Image(getWorkbench().getDisplay(), data);
            getImageRegistry().put(COLLISION_GROUP_IMAGE_ID + i, image);
        }
    }
}

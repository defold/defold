
package com.dynamo.cr.tileeditor;

import java.awt.Color;

import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.graphics.ImageData;
import org.eclipse.swt.graphics.PaletteData;
import org.eclipse.swt.graphics.RGB;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.editor.ui.AbstractDefoldPlugin;
import com.dynamo.cr.editor.ui.ColorUtil;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractDefoldPlugin {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.tileeditor"; //$NON-NLS-1$

    // Context IDs
    public static final String TILE_SET_CONTEXT_ID = "com.dynamo.cr.tileeditor.contexts.TileSetEditor"; //$NON-NLS-1$
    public static final String GRID_CONTEXT_ID = "com.dynamo.cr.tileeditor.contexts.GridEditor"; //$NON-NLS-1$
    public static final String ATLAS_CONTEXT_ID = "com.dynamo.cr.tileeditor.contexts.AtlasEditor"; //$NON-NLS-1$

    // Image ids
    public static final String COLLISION_GROUP_IMAGE_ID = "COLLISION_GROUP"; //$NON-NLS-1$
    public static final String GRID_IMAGE_ID = "GRID"; //$NON-NLS-1$
    public static final String LAYER_IMAGE_ID = "LAYER"; //$NON-NLS-1$
    public static final String ANIMATION_IMAGE_ID = "ANIMATION"; //$NON-NLS-1$
    public static final String IMAGE_IMAGE_ID = "IMAGE"; //$NON-NLS-1$

    // Collision groups
    public static final int MAX_COLLISION_GROUP_COUNT = 16;


    // The shared instance
    private static Activator plugin;

    private Color[] collisionGroupColors;

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

    /**
     * Fetch collision group specific image.
     * Must be called from UI thread.
     * @param index Collision group index
     * @return Collision group specific index, or generic if index is out of bounds
     */
    public Image getCollisionGroupImage(int index) {
        if (index >= 0) {
            String imageKey = COLLISION_GROUP_IMAGE_ID + index;
            return getImageRegistry().get(imageKey);
        }
        return getImageRegistry().get(COLLISION_GROUP_IMAGE_ID);
    }

    public Color getCollisionGroupColor(int index) {
        if (this.collisionGroupColors == null) {
            generateCollisionGroupColors();
        }
        if (index >= 0) {
            return this.collisionGroupColors[index];
        }
        return null;
    }

    @Override
    protected void initializeImageRegistry(ImageRegistry registry) {
        super.initializeImageRegistry(registry);

        registry.put(COLLISION_GROUP_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/collision_group.png"));
        registry.put(GRID_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/tile_grid.png"));
        registry.put(LAYER_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/layer.png"));
        registry.put(ANIMATION_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/film.png"));
        registry.put(IMAGE_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/image.png"));

        generateCollisionGroupColors();
        generateCollisionGroupIcons();
    }

    private void generateCollisionGroupColors() {
        if (this.collisionGroupColors == null) {
            this.collisionGroupColors = new Color[MAX_COLLISION_GROUP_COUNT];
        }
        int index = 0;
        Color[] c = this.collisionGroupColors;
        c[index++] = ColorUtil.createColorFromHueAlpha(0.0f, 0.7f);
        generateCollisionGroupColor(c, 0.5f, index);
    }

    private static void generateCollisionGroupColor(Color[] c, float fraction, int index) {
        int n = MAX_COLLISION_GROUP_COUNT;
        for (float f = fraction; index < n && f < 1.0f; f += 2.0f * fraction) {
            c[index++] = ColorUtil.createColorFromHueAlpha(f * 360.0f, 0.7f);
        }
        if (index < n) {
            generateCollisionGroupColor(c, fraction * 0.5f, index);
        }
    }

    private void generateCollisionGroupIcons() {
        int n = MAX_COLLISION_GROUP_COUNT;
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

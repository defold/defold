package com.dynamo.cr.sceneed.ui;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.net.URL;
import java.util.HashMap;
import java.util.Map;

import javax.media.opengl.GLException;

import com.sun.opengl.util.texture.Texture;
import com.sun.opengl.util.texture.TextureIO;

/**
 * Texture registry. Currently only used to load classpath-based resource
 * Could be extended to support regular file-loading as well
 * @author chmu
 *
 */
public class TextureRegistry {

    private Map<String, Texture> textures = new HashMap<String, Texture>();
    private Texture defaultTexture;

    private Texture getDefaultTexture() {
        if (defaultTexture != null) {
            return defaultTexture;
        }

        BufferedImage image = new BufferedImage(2, 2, BufferedImage.TYPE_3BYTE_BGR);
        image.setRGB(0, 0, 0);
        image.setRGB(1, 0, 0xffffff);
        image.setRGB(0, 1, 0xffffff);
        image.setRGB(1, 1, 0);
        defaultTexture = TextureIO.newTexture(image, true);
        return defaultTexture;
    }

    /**
     * Load and create a texture from URL. The URL is typically a resource-uri. See {@link Class#getResource(String)}
     * @note Textures are cached
     * @param url URL to get resource for.
     * @return texture or default texture if texture isn't found (checker)
     */
    public Texture getResourceTexture(URL url) {
        if (url == null) {
            return getDefaultTexture();
        }

        Texture texture = null;

        String urlString = url.toString();

        if (textures.containsKey(urlString)) {
            return textures.get(urlString);
        }

        String path = url.getPath();
        String[] lst = path.split("\\.");
        String ext = lst[lst.length-1];
        try {
            texture = TextureIO.newTexture(url, true, ext);
        } catch (GLException e) {
            // TODO: See case 1331
            e.printStackTrace();
        } catch (IOException e) {
            // TODO: See case 1331
            e.printStackTrace();
        }

        if (texture == null) {
            texture = getDefaultTexture();
        }

        textures.put(urlString, texture);

        return texture;
    }

}

package com.dynamo.cr.tileeditor.atlas;

import java.awt.Graphics;
import java.awt.image.BufferedImage;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.cr.tileeditor.atlas.AtlasLayout.Layout;
import com.dynamo.cr.tileeditor.atlas.AtlasLayout.LayoutType;
import com.dynamo.cr.tileeditor.atlas.AtlasLayout.Rect;
import com.dynamo.cr.tileeditor.atlas.AtlasMap.Tile;

public class AtlasGenerator {

    public static AtlasMap generate(List<BufferedImage> images, List<String> ids, int margin) {
        List<AtlasLayout.Rect> rectangles = new ArrayList<AtlasLayout.Rect>(images.size());
        int i = 0;
        for (BufferedImage image : images) {
            rectangles.add(new Rect(i++, image.getWidth(), image.getHeight()));
        }

        Layout layout = AtlasLayout.layout(LayoutType.BASIC, margin, rectangles);

        List<AtlasMap.Tile> tiles = new ArrayList<AtlasMap.Tile>(layout.getRectangles().size());

        BufferedImage image = new BufferedImage(layout.getWidth(), layout.getHeight(), BufferedImage.TYPE_4BYTE_ABGR);
        Graphics g = image.getGraphics();
        float xs = 1.0f / image.getWidth();
        float ys = 1.0f / image.getHeight();
        for (Rect r : layout.getRectangles()) {
            int index = (Integer) r.id;
            g.drawImage(images.get(index), r.x, r.y, null);
            float x0 = r.x;
            float y0 = r.y;
            float x1 = r.x + r.width;
            float y1 = r.y + r.height;
            float w2 = r.width * 0.5f;
            float h2 = r.height * 0.5f;
            float[] vertices = new float[] {
                    x0 * xs, y1 * ys, -w2, -h2, 0,
                    x1 * xs, y1 * ys, w2, -h2, 0,
                    x1 * xs, y0 * ys, w2, h2, 0,

                    x0 * xs, y1 * ys, -w2, -h2, 0,
                    x1 * xs, y0 * ys, w2, h2, 0,
                    x0 * xs, y0 * ys, -w2, h2, 0,

            };
            float cx = r.x + r.width * 0.5f;
            float cy = r.y + r.height * 0.5f;
            tiles.add(new Tile(ids.get(index), cx, cy, vertices));
        }
        g.dispose();
        AtlasMap atlasMap = new AtlasMap(image, tiles);
        return atlasMap;
    }

}

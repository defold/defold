package com.dynamo.cr.tileeditor.core;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.beans.PropertyChangeEvent;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Point3f;
import javax.vecmath.Vector3f;

import org.eclipse.core.runtime.IProgressMonitor;

import com.dynamo.cr.tileeditor.operations.AddCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.RemoveCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.RenameCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.SetConvexHullCollisionGroupOperation;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetPresenter implements TaggedPropertyListener {
    private final TileSetModel model;
    private final ITileSetView view;
    private List<Color> collisionGroupColors;

    public TileSetPresenter(TileSetModel model, ITileSetView view) {
        this.model = model;
        this.view = view;
        this.model.addTaggedPropertyListener(this);
        this.collisionGroupColors = new ArrayList<Color>();
    }

    public void load(TileSet tileSet) throws IOException {
        this.model.load(tileSet);
    }

    public void save(OutputStream outputStream, IProgressMonitor monitor) throws IOException {
        this.model.save(outputStream, monitor);
    }

    public TileSetModel getModel() {
        return this.model;
    }

    public void setConvexHullCollisionGroup(int index, String collisionGroup) {
        this.model.executeOperation(new SetConvexHullCollisionGroupOperation(this.model, index, collisionGroup));
    }

    public void addCollisionGroup(String collisionGroup) {
        this.model.executeOperation(new AddCollisionGroupOperation(this.model, collisionGroup));
    }

    public void removeCollisionGroup(String collisionGroup) {
        this.model.executeOperation(new RemoveCollisionGroupOperation(this.model, collisionGroup));
    }

    public void renameCollisionGroup(String collisionGroup, String newCollisionGroup) {
        this.model.executeOperation(new RenameCollisionGroupOperation(this.model, collisionGroup, newCollisionGroup));
    }

    @SuppressWarnings({ "unchecked" })
    @Override
    public void propertyChange(PropertyChangeEvent evt) {
        if (evt.getSource() instanceof TileSetModel) {
            if (evt.getPropertyName().equals("image")) {
                view.setImageProperty((String)evt.getNewValue());
            } else if (evt.getPropertyName().equals("tileWidth")) {
                view.setTileWidthProperty((Integer)evt.getNewValue());
            } else if (evt.getPropertyName().equals("tileHeight")) {
                view.setTileHeightProperty((Integer)evt.getNewValue());
            } else if (evt.getPropertyName().equals("tileMargin")) {
                view.setTileMarginProperty((Integer)evt.getNewValue());
            } else if (evt.getPropertyName().equals("tileSpacing")) {
                view.setTileSpacingProperty((Integer)evt.getNewValue());
            } else if (evt.getPropertyName().equals("collision")) {
                view.setCollisionProperty((String)evt.getNewValue());
            } else if (evt.getPropertyName().equals("materialTag")) {
                view.setMaterialTagProperty((String)evt.getNewValue());
            } else if (evt.getPropertyName().equals("collisionGroups")) {
                setViewCollisionGroups((List<String>)evt.getNewValue());
            } else if (evt.getPropertyName().equals("convexHulls")) {
                setViewTiles((List<TileSetModel.ConvexHull>)evt.getNewValue());
            }
        } else if (evt.getSource() instanceof TileSetModel.ConvexHull) {
            if (evt.getPropertyName().equals("collisionGroup")) {
                setViewHullColor((TileSetModel.ConvexHull)evt.getSource(), (String)evt.getNewValue());
            }
        }
    }

    @Override
    public void propertyTag(PropertyTagEvent evt) {
        if (evt.getSource() instanceof TileSetModel) {
            if (evt.getPropertyName().equals("image")) {
                this.view.setImageTags(evt.getTags());
            }
        }
    }

    private void setViewTiles(List<TileSetModel.ConvexHull> convexHulls) {
        BufferedImage image = this.model.getLoadedImage();
        int tilesPerRow = 0;
        int tilesPerColumn = 0;
        if (image != null) {
            tilesPerRow = this.model.calcTileCount(this.model.getTileWidth(), image.getWidth());
            tilesPerColumn = this.model.calcTileCount(this.model.getTileHeight(), image.getHeight());
        }
        if (tilesPerRow > 0 && tilesPerColumn > 0) {
            updateCollisionGroupColors(this.model.getCollisionGroups().size());
            int tileCount = tilesPerRow * tilesPerColumn;
            int tileWidth = this.model.getTileWidth();
            int tileHeight = this.model.getTileHeight();
            int tileMargin = this.model.getTileMargin();
            int tileSpacing = this.model.getTileSpacing();
            // vertex data is 3 components for position and 2 for uv
            int vertexComponentCount = 5;
            float[] v = new float[vertexComponentCount * 6 * tileCount];
            // render space is a quad of [0,1],[0,1] into which the tiles are rendered with one pixel spacing
            int pixelWidth = tilesPerRow * (1 + tileWidth) + 1;
            float recipPixelWidth = 1.0f / pixelWidth;
            int pixelHeight = tilesPerColumn * (1 + tileHeight) + 1;
            float recipPixelHeight = 1.0f / pixelHeight;
            float recipImageWidth = 1.0f / image.getWidth();
            float recipImageHeight = 1.0f / image.getHeight();
            float z = 0.0f;
            int i = 0;
            List<Point3f> hullOffsets = new ArrayList<Point3f>(tileCount);
            for (int row = 0; row < tilesPerColumn; ++row) {
                for (int column = 0; column < tilesPerRow; ++column) {
                    float x0 = (column * (1 + tileWidth) + 1) * recipPixelWidth;
                    float x1 = (column + 1) * (1 + tileWidth) * recipPixelWidth;
                    float y0 = (row * (1 + tileHeight) + 1) * recipPixelHeight;
                    float y1 = (row + 1) * (1 + tileHeight) * recipPixelHeight;
                    float u0 = (column * (tileSpacing + tileMargin + tileWidth) + tileSpacing + tileMargin + 0.5f) * recipImageWidth;
                    float u1 = ((column + 1) * (tileSpacing + tileMargin + tileWidth) - 0.5f) * recipImageWidth;
                    float v0 = ((row + 1) * (tileSpacing + tileMargin + tileHeight) - 0.5f) * recipImageHeight;
                    float v1 = (row * (tileSpacing + tileMargin + tileHeight) + tileSpacing + tileMargin + 0.5f) * recipImageHeight;
                    v[i+0] = x0; v[i+1] = y0; v[i+2] = z; v[i+3] = u0; v[i+4] = v0;
                    i += vertexComponentCount;
                    v[i+0] = x0; v[i+1] = y1; v[i+2] = z; v[i+3] = u0; v[i+4] = v1;
                    i += vertexComponentCount;
                    v[i+0] = x1; v[i+1] = y0; v[i+2] = z; v[i+3] = u1; v[i+4] = v0;
                    i += vertexComponentCount;
                    v[i+0] = x1; v[i+1] = y0; v[i+2] = z; v[i+3] = u1; v[i+4] = v0;
                    i += vertexComponentCount;
                    v[i+0] = x0; v[i+1] = y1; v[i+2] = z; v[i+3] = u0; v[i+4] = v1;
                    i += vertexComponentCount;
                    v[i+0] = x1; v[i+1] = y1; v[i+2] = z; v[i+3] = u1; v[i+4] = v1;
                    i += vertexComponentCount;
                    hullOffsets.add(new Point3f(x0 + 0.5f, y0 + 0.5f, z));
                }
            }
            int[] hullIndices = new int[tileCount];
            int[] hullCounts = new int[tileCount];
            Color[] hullColors = new Color[tileCount];
            for (int tile = 0; tile < tileCount; ++tile) {
                TileSetModel.ConvexHull hull = this.model.getConvexHulls().get(tile);
                hullIndices[tile] = hull.getIndex();
                hullCounts[tile] = hull.getCount();
                hullColors[tile] = Color.white;
                if (hull.getCount() > 0) {
                    int collisionGroupIndex = this.model.getCollisionGroups().indexOf(hull.getCollisionGroup());
                    if (collisionGroupIndex > 0) {
                        hullColors[tile] = this.collisionGroupColors.get(collisionGroupIndex);
                    }
                }
            }
            Vector3f hullScale = new Vector3f(recipPixelWidth, recipPixelHeight, 1.0f);
            view.setTiles(image, v, hullIndices, hullCounts, hullColors, hullScale);
        } else {
            view.clearTiles();
        }
    }

    private void setViewHullColor(TileSetModel.ConvexHull hull, String collisionGroup) {
        int tileIndex = this.model.getConvexHulls().indexOf(hull);
        if (tileIndex < 0) {
            // TODO: Report error? Only cause imo would be an event pointing to an old hull, should never happen.
            return;
        }
        int collisionGroupIndex = this.model.getCollisionGroups().indexOf(hull.getCollisionGroup());
        Color color = Color.white;
        if (collisionGroupIndex > 0) {
            color = this.collisionGroupColors.get(collisionGroupIndex);
        }
        this.view.setTileHullColor(tileIndex, color);
    }

    private void setViewCollisionGroups(List<String> collisionGroups) {
        updateCollisionGroupColors(collisionGroups.size());
        view.setCollisionGroups(collisionGroups, this.collisionGroupColors);
    }

    private void updateCollisionGroupColors(int size) {
        if (this.collisionGroupColors.size() != size) {
            this.collisionGroupColors = new ArrayList<Color>(size);
            float recip_size = 1.0f/size;
            for (int i = 0; i < size; ++i) {
                float h = i * recip_size * 360;
                this.collisionGroupColors.add(generateColorFromHue(h));
            }
        }
    }

    private Color generateColorFromHue(float hue) {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        float h_p = hue / 60.0f;
        float c = 1.0f;
        float x = c * (1.0f - Math.abs((h_p % 2.0f) - 1.0f));
        int type = (int)h_p;
        switch (type) {
        case 0: r = c; g = x; break;
        case 1: r = x; g = c; break;
        case 2: g = c; b = x; break;
        case 3: g = x; b = c; break;
        case 4: r = x; b = c; break;
        case 5: r = c; b = x; break;
        }
        return new Color(r, g, b);
    }

}

package com.dynamo.cr.tileeditor.core;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.beans.PropertyChangeEvent;
import java.io.IOException;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.ui.services.IDisposable;

import com.dynamo.cr.tileeditor.operations.AddCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.RemoveCollisionGroupsOperation;
import com.dynamo.cr.tileeditor.operations.RenameCollisionGroupsOperation;
import com.dynamo.cr.tileeditor.operations.SetConvexHullCollisionGroupsOperation;
import com.dynamo.tile.proto.Tile.TileSet;

public class TileSetPresenter implements TaggedPropertyListener, IOperationHistoryListener, IDisposable {
    private final TileSetModel model;
    private final ITileSetView view;
    private List<Color> collisionGroupColors;
    // Used for painting collision groups onto tiles (convex hulls)
    private String[] oldCollisionGroups;
    private String currentCollisionGroup;
    private int undoRedoCounter;

    public TileSetPresenter(TileSetModel model, ITileSetView view) {
        this.model = model;
        this.view = view;
        this.model.addTaggedPropertyListener(this);
        this.collisionGroupColors = new ArrayList<Color>();
        this.undoRedoCounter = 0;
        this.model.getUndoHistory().addOperationHistoryListener(this);
    }

    @Override
    public void dispose() {
        this.model.removeTaggedPropertyListener(this);
        this.model.getUndoHistory().removeOperationHistoryListener(this);
    }

    public void load(TileSet tileSet) throws IOException {
        this.model.load(tileSet);
        setUndoRedoCounter(0);
    }

    public void save(OutputStream outputStream, IProgressMonitor monitor) throws IOException {
        this.model.save(outputStream, monitor);
        setUndoRedoCounter(0);
    }

    public TileSetModel getModel() {
        return this.model;
    }

    public void beginSetConvexHullCollisionGroup(String collisionGroup) {
        int n = this.model.getConvexHulls().size();
        if (n > 0) {
            if (this.oldCollisionGroups == null || this.oldCollisionGroups.length != n) {
                this.oldCollisionGroups = new String[n];
            } else {
                for (int i = 0; i < n; ++i) {
                    this.oldCollisionGroups[i] = null;
                }
            }
            this.currentCollisionGroup = collisionGroup;
        }
    }

    public void endSetConvexHullCollisionGroup() {
        if (this.currentCollisionGroup != null) {
            this.model.executeOperation(new SetConvexHullCollisionGroupsOperation(this.model, this.oldCollisionGroups, this.currentCollisionGroup));
            this.currentCollisionGroup = null;
        }
    }

    public void setConvexHullCollisionGroup(int index) {
        if (this.oldCollisionGroups != null && this.currentCollisionGroup != null) {
            TileSetModel.ConvexHull convexHull = this.model.getConvexHulls().get(index);
            if (this.oldCollisionGroups[index] == null) {
                this.oldCollisionGroups[index] = convexHull.getCollisionGroup();
            }
            convexHull.setCollisionGroup(currentCollisionGroup);
        }
    }

    public void addCollisionGroup(String collisionGroup) {
        this.model.executeOperation(new AddCollisionGroupOperation(this.model, collisionGroup));
    }

    public void removeSelectedCollisionGroups() {
        if (this.model.getSelectedCollisionGroups().length > 0) {
            this.model.executeOperation(new RemoveCollisionGroupsOperation(this.model));
        }
    }

    public void renameSelectedCollisionGroups(String[] newCollisionGroups) {
        if (this.model.getSelectedCollisionGroups().length == newCollisionGroups.length) {
            this.model.executeOperation(new RenameCollisionGroupsOperation(this.model, newCollisionGroups));
        }
    }

    public void selectCollisionGroups(String[] selectedCollisionGroups) {
        this.model.setSelectedCollisionGroups(selectedCollisionGroups);
    }

    public void refresh() {
        this.view.refreshProperties();
        setViewCollisionGroups(this.model.getCollisionGroups());
        setViewTiles(this.model.getConvexHulls());
    }

    @SuppressWarnings({ "unchecked" })
    @Override
    public void propertyChange(PropertyChangeEvent evt) {
        if (evt.getSource() instanceof TileSetModel) {
            if (evt.getPropertyName().equals("collisionGroups")) {
                setViewCollisionGroups((List<String>)evt.getNewValue());
            } else if (evt.getPropertyName().equals("convexHulls")) {
                setViewTiles((List<TileSetModel.ConvexHull>)evt.getNewValue());
            } else {
                this.view.refreshProperties();
                // TODO: Solve this better
                setViewTiles(this.model.getConvexHulls());
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
            this.view.refreshProperties();
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        int type = event.getEventType();
        switch (type) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.REDONE:
            setUndoRedoCounter(this.undoRedoCounter+1);
            break;
        case OperationHistoryEvent.UNDONE:
            setUndoRedoCounter(this.undoRedoCounter-1);
            break;
        }
    }

    private void setUndoRedoCounter(int undoRedoCounter) {
        boolean prevDirty = this.undoRedoCounter != 0;
        boolean dirty = undoRedoCounter != 0;
        if (prevDirty != dirty) {
            this.view.setDirty(dirty);
        }
        this.undoRedoCounter = undoRedoCounter;
    }

    private void setViewTiles(List<TileSetModel.ConvexHull> convexHulls) {
        BufferedImage image = this.model.getLoadedImage();
        BufferedImage collision = this.model.getLoadedCollision();
        if (image == null && collision == null) {
            this.view.clearTiles();
            return;
        }
        int width = 0;
        int height = 0;
        if (image != null) {
            width = image.getWidth();
            height = image.getHeight();
        } else {
            width = collision.getWidth();
            height = collision.getHeight();
        }
        int tileWidth = this.model.getTileWidth();
        int tileHeight = this.model.getTileHeight();
        int tileMargin = this.model.getTileMargin();
        int tileSpacing = this.model.getTileSpacing();
        int tilesPerRow = TileSetUtil.calculateTileCount(tileWidth, width, tileMargin, tileSpacing);
        int tilesPerColumn = TileSetUtil.calculateTileCount(tileHeight, height, tileMargin, tileSpacing);

        int tileCount = tilesPerRow * tilesPerColumn;
        if (tilesPerRow > 0 && tilesPerColumn > 0 && tileCount == this.model.getConvexHulls().size()) {
            updateCollisionGroupColors(this.model.getCollisionGroups().size());
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
            this.view.setTileData(image, collision, tileWidth, tileHeight, tileMargin, tileSpacing, this.model.getConvexHullPoints(), hullIndices, hullCounts, hullColors);
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
        view.setCollisionGroups(collisionGroups, this.collisionGroupColors, this.model.getSelectedCollisionGroups());
    }

    private void updateCollisionGroupColors(int size) {
        if (this.collisionGroupColors.size() != size) {
            this.collisionGroupColors = new ArrayList<Color>(size);
            float recip_size = 1.0f/size;
            float alpha = 0.7f;
            for (int i = 0; i < size; ++i) {
                float h = i * recip_size * 360;
                this.collisionGroupColors.add(generateColorFromHue(h, alpha));
            }
        }
    }

    private Color generateColorFromHue(float hue, float alpha) {
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
        return new Color(r, g, b, alpha);
    }

}

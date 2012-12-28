package com.dynamo.cr.tileeditor.core;

import java.awt.Color;
import java.awt.image.BufferedImage;
import java.beans.PropertyChangeEvent;
import java.beans.PropertyChangeListener;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.ui.services.IDisposable;

import com.dynamo.bob.tile.ConvexHull;
import com.dynamo.bob.tile.TileSetUtil;
import com.dynamo.cr.tileeditor.operations.AddCollisionGroupOperation;
import com.dynamo.cr.tileeditor.operations.RemoveCollisionGroupsOperation;
import com.dynamo.cr.tileeditor.operations.RenameCollisionGroupsOperation;
import com.dynamo.cr.tileeditor.operations.SetConvexHullsOperation;

public class TileSetPresenter implements PropertyChangeListener, IOperationHistoryListener, IDisposable {
    private final TileSetModel model;
    private final ITileSetView view;
    private List<Color> collisionGroupColors;
    // Used for painting collision groups onto tiles (convex hulls)
    private List<ConvexHull> oldConvexHulls;
    private String currentCollisionGroup;
    private int undoRedoCounter;
    private boolean loading = false;

    public TileSetPresenter(TileSetModel model, ITileSetView view) {
        this.model = model;
        this.view = view;
        this.model.addTaggedPropertyListener(this);
        this.collisionGroupColors = new ArrayList<Color>();
        this.undoRedoCounter = 0;
        this.model.getUndoHistory().addOperationHistoryListener(this);
    }

    public void handleResourceChanged(IResourceChangeEvent event) {
        if (this.model.handleResourceChanged(event))
            refresh();
    }

    @Override
    public void dispose() {
        this.model.removeTaggedPropertyListener(this);
        this.model.getUndoHistory().removeOperationHistoryListener(this);
    }

    public void load(InputStream is) throws IOException {
        loading = true;
        try {
            this.model.load(is);
        } finally {
            loading = false;
        }
        refresh();
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
        this.oldConvexHulls = this.model.getConvexHulls();
        this.currentCollisionGroup = collisionGroup;
    }

    public void endSetConvexHullCollisionGroup() {
        if (this.currentCollisionGroup != null) {
            List<ConvexHull> convexHulls = this.model.getConvexHulls();
            if (!this.oldConvexHulls.equals(convexHulls)) {
                this.model.executeOperation(new SetConvexHullsOperation(this.model, this.oldConvexHulls, convexHulls, this.currentCollisionGroup));
            }
            this.currentCollisionGroup = null;
        }
    }

    public void setConvexHullCollisionGroup(int index) {
        if (this.currentCollisionGroup != null) {
            List<ConvexHull> convexHulls = this.model.getConvexHulls();
            ConvexHull convexHull = convexHulls.get(index);
            convexHull.setCollisionGroup(this.currentCollisionGroup);
            this.model.setConvexHull(index, convexHull);
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
        this.view.setImage(this.model.getLoadedImage());
        this.view.setTileWidth(this.model.getTileWidth());
        this.view.setTileHeight(this.model.getTileHeight());
        this.view.setTileMargin(this.model.getTileMargin());
        this.view.setTileSpacing(this.model.getTileSpacing());
        this.view.setCollision(this.model.getLoadedCollision());
        this.view.refreshProperties();
        setViewCollisionGroups(this.model.getCollisionGroups());
        setViewHulls(this.model.getConvexHulls());
        this.view.setValid(this.model.validate().getSeverity() <= IStatus.INFO);
    }

    @SuppressWarnings({ "unchecked" })
    @Override
    public void propertyChange(PropertyChangeEvent evt) {
        if (loading)
            return;
        this.view.setValid(this.model.validate().getSeverity() <= IStatus.INFO);
        if (evt.getNewValue() instanceof IStatus) {
            this.view.refreshProperties();
        } else {
            if (evt.getSource() instanceof TileSetModel) {
                if (evt.getPropertyName().equals("collisionGroups")) {
                    List<String> newGroups = (List<String>)evt.getNewValue();
                    setViewCollisionGroups(newGroups);
                    setViewHulls(this.model.getConvexHulls());
                } else if (evt.getPropertyName().equals("convexHulls")) {
                    setViewHulls((List<ConvexHull>)evt.getNewValue());
                } else {
                    if (evt.getPropertyName().equals("image")) {
                        this.view.setImage(this.model.getLoadedImage());
                    } else if (evt.getPropertyName().equals("tileWidth")) {
                        this.view.setTileWidth((Integer)evt.getNewValue());
                    } else if (evt.getPropertyName().equals("tileHeight")) {
                        this.view.setTileHeight((Integer)evt.getNewValue());
                    } else if (evt.getPropertyName().equals("tileMargin")) {
                        this.view.setTileMargin((Integer)evt.getNewValue());
                    } else if (evt.getPropertyName().equals("tileSpacing")) {
                        this.view.setTileSpacing((Integer)evt.getNewValue());
                    } else if (evt.getPropertyName().equals("collision")) {
                        this.view.setCollision(this.model.getLoadedCollision());
                    }

                    this.view.refreshProperties();
                }
            }
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (!event.getOperation().hasContext(this.model.getUndoContext())) {
            // Only handle operations related to this editor
            return;
        }
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

    private void setViewHulls(List<ConvexHull> convexHulls) {
        BufferedImage collision = this.model.getLoadedCollision();
        if (collision != null) {
            int[] hullIndices = null;
            int[] hullCounts = null;
            Color[] hullColors = null;

            int width = collision.getWidth();
            int height = collision.getHeight();
            int tileWidth = this.model.getTileWidth();
            int tileHeight = this.model.getTileHeight();
            int tileMargin = this.model.getTileMargin();
            int tileSpacing = this.model.getTileSpacing();
            int tilesPerRow = TileSetUtil.calculateTileCount(tileWidth, width, tileMargin, tileSpacing);
            int tilesPerColumn = TileSetUtil.calculateTileCount(tileHeight, height, tileMargin, tileSpacing);

            int tileCount = tilesPerRow * tilesPerColumn;
            if (tilesPerRow > 0 && tilesPerColumn > 0 && tileCount == convexHulls.size()) {
                updateCollisionGroupColors(this.model.getCollisionGroups().size());
                hullIndices = new int[tileCount];
                hullCounts = new int[tileCount];
                hullColors = new Color[tileCount];
                for (int tile = 0; tile < tileCount; ++tile) {
                    ConvexHull hull = convexHulls.get(tile);
                    hullIndices[tile] = hull.getIndex();
                    hullCounts[tile] = hull.getCount();
                    hullColors[tile] = Color.white;
                    if (hull.getCount() > 0) {
                        int collisionGroupIndex = this.model.getCollisionGroups().indexOf(hull.getCollisionGroup());
                        if (collisionGroupIndex >= 0) {
                            hullColors[tile] = this.collisionGroupColors.get(collisionGroupIndex);
                        }
                    }
                }
            }
            this.view.setHulls(this.model.getConvexHullPoints(), hullIndices, hullCounts, hullColors);
        }
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

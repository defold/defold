package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.runtime.IAdaptable;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorModel;

@Entity(commandFactory = GridUndoableCommandFactory.class)
public class Layer implements IAdaptable {

    public static class Cell {

        private final int tile;
        private final boolean hFlip;
        private final boolean vFlip;

        public Cell(int tile, boolean hFlip, boolean vFlip) {
            this.tile = tile;
            this.hFlip = hFlip;
            this.vFlip = vFlip;
        }

        public int getTile() {
            return this.tile;
        }

        public boolean isHFlip() {
            return this.hFlip;
        }

        public boolean isVFlip() {
            return this.vFlip;
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof Cell) {
                Cell cell = (Cell)obj;
                return this.tile == cell.tile
                        && this.hFlip == cell.hFlip
                        && this.vFlip == cell.vFlip;
            }
            return super.equals(obj);
        }

    }

    @Property
    private String id;
    @Property
    private float z;
    @Property
    private boolean visible;

    private GridModel gridModel;

    // upper 32-bits y, lower 32-bits x
    private Map<Long, Cell> cells = new HashMap<Long, Cell>();

    public Layer() {
    }

    public Layer(Layer layer) {
        this.id = layer.id;
        this.z = layer.z;
        this.visible = layer.visible;
        this.gridModel = layer.gridModel;
        this.cells = new HashMap<Long, Cell>(layer.cells);
    }

    public String getId() {
        return this.id;
    }

    public void setId(String id) {
        if ((this.id == null && id != null) || (this.id != null && !this.id.equals(id))) {
            String oldId = this.id;
            this.id = id;
            if (this.gridModel != null)
                this.gridModel.firePropertyChangeEvent(new PropertyChangeEvent(this, "id", oldId, id));
        }
    }

    public float getZ() {
        return this.z;
    }

    public void setZ(float z) {
        if (this.z != z) {
            float oldZ = this.z;
            this.z = z;
            if (gridModel != null)
                gridModel.firePropertyChangeEvent(new PropertyChangeEvent(this, "z", oldZ, z));
        }
    }

    public boolean isVisible() {
        return this.visible;
    }

    public void setVisible(boolean visible) {
        if (this.visible != visible) {
            boolean oldVisible = this.visible;
            this.visible = visible;
            if (gridModel != null)
                gridModel.firePropertyChangeEvent(new PropertyChangeEvent(this, "visible", oldVisible, visible));
        }
    }

    public void setGridModel(GridModel gridModel) {
        this.gridModel = gridModel;
    }

    public Map<Long, Cell> getCells() {
        return Collections.unmodifiableMap(this.cells);
    }

    public void setCells(Map<Long, Cell> cells) {
        this.cells = new HashMap<Long, Cell>(cells);
    }

    public Cell getCell(long cellIndex) {
        return this.cells.get(cellIndex);
    }

    public Cell getCell(int x, int y) {
        return getCell(toCellIndex(x, y));
    }

    public void setCell(long cellIndex, Cell cell) {
        this.cells.put(cellIndex, cell);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof Layer) {
            Layer layer = (Layer)obj;
            return this.id.equals(layer.id)
                    && this.z == layer.z
                    && this.visible == layer.visible;
        } else {
            return super.equals(obj);
        }
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<Layer, GridModel>(this, gridModel, GridModel.layerIntrospector);
        }
        return null;
    }

    public static long toCellIndex(int x, int y) {
        return (((long)y) << Integer.SIZE) | (x & 0xFFFFFFFFL);
    }

    public static int toCellX(long index) {
        return (int)index;
    }

    public static int toCellY(long index) {
        return (int)(index >>> Integer.SIZE);
    }

}

package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.runtime.IAdaptable;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileGrid;

@Entity(commandFactory = UndoableCommandFactory.class)
public class GridModel extends Model implements IPropertyObjectWorld, IAdaptable {

    private static PropertyIntrospector<Cell, GridModel> cellIntrospector = new PropertyIntrospector<Cell, GridModel>(Cell.class);
    public class Cell implements IAdaptable {

        @Override
        public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
            if (adapter == IPropertyModel.class) {
                return new PropertyIntrospectorModel<Cell, GridModel>(this, GridModel.this, cellIntrospector, null);
            }
            return null;
        }

        @Property
        private int tile;
        @Property
        private boolean hFlip;
        @Property
        private boolean vFlip;

        public int getTile() {
            return tile;
        }
        public void setTile(int tile) {
            this.tile = tile;
        }
        public boolean ishFlip() {
            return hFlip;
        }
        public void sethFlip(boolean hFlip) {
            this.hFlip = hFlip;
        }
        public boolean isvFlip() {
            return vFlip;
        }
        public void setvFlip(boolean vFlip) {
            this.vFlip = vFlip;
        }

    }

    private static PropertyIntrospector<Layer, GridModel> layerIntrospector = new PropertyIntrospector<Layer, GridModel>(Layer.class);
    public class Layer implements IAdaptable {

        @Override
        public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
            if (adapter == IPropertyModel.class) {
                return new PropertyIntrospectorModel<Layer, GridModel>(this, GridModel.this, layerIntrospector, null);
            }
            return null;
        }

        @Property
        private String id;
        @Property
        private float z;
        @Property
        private boolean visible;

        // upper 32-bits y, lower 32-bits x
        private final Map<Long, Cell> cells = new HashMap<Long, GridModel.Cell>();

        public Layer() {
        }

        public String getId() {
            return this.id;
        }

        public void setId(String id) {
            if ((this.id == null && id != null) || !this.id.equals(id)) {
                String oldId = this.id;
                this.id = id;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "id", oldId, id));
            }
        }

        public float getZ() {
            return this.z;
        }

        public void setZ(float z) {
            if (this.z != z) {
                float oldZ = this.z;
                this.z = z;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "z", oldZ, z));
            }
        }

        public boolean isVisible() {
            return this.visible;
        }

        public void setVisible(boolean visible) {
            if (this.visible != visible) {
                boolean oldVisible = this.visible;
                this.visible = visible;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "visible", oldVisible, visible));
            }
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

    }

    @Property(isResource = true)
    private String tileSet;
    @Property
    private float cellWidth;
    @Property
    private float cellHeight;

    private List<Layer> layers;

    private final IOperationHistory history;
    private final IUndoContext undoContext;

    public GridModel(IOperationHistory history, IUndoContext undoContext) {
        this.history = history;
        this.undoContext = undoContext;

        this.layers = new ArrayList<Layer>();
    }

    public String getTileSet() {
        return this.tileSet;
    }

    public void setTileSet(String tileSet) {
        if ((this.tileSet == null && tileSet != null) || !this.tileSet.equals(tileSet)) {
            String oldTileSet = this.tileSet;
            this.tileSet = tileSet;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileSet", oldTileSet, tileSet));
        }
    }

    public float getCellWidth() {
        return this.cellWidth;
    }

    public void setCellWidth(float cellWidth) {
        if (this.cellWidth != cellWidth) {
            float oldCellWidth = this.cellWidth;
            this.cellWidth = cellWidth;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "cellWidth", new Float(oldCellWidth), new Float(cellWidth)));
        }
    }

    public float getCellHeight() {
        return this.cellHeight;
    }

    public void setCellHeight(float cellHeight) {
        if (this.cellHeight != cellHeight) {
            float oldCellHeight = this.cellHeight;
            this.cellHeight = cellHeight;
            firePropertyChangeEvent(new PropertyChangeEvent(this, "cellHeight", new Float(oldCellHeight), new Float(cellHeight)));
        }
    }

    public List<Layer> getLayers() {
        return this.layers;
    }

    public void setLayers(List<Layer> layers) {
        if (this.layers != layers) {
            boolean equal = true;
            if (this.layers.size() == layers.size()) {
                int n = this.layers.size();
                for (int i = 0; i < n; ++i) {
                    if (!this.layers.get(i).equals(layers.get(i))) {
                        equal = false;
                        break;
                    }
                }
            } else {
                equal = false;
            }
            if (!equal) {
                List<Layer> oldLayers = this.layers;
                this.layers = layers;
                firePropertyChangeEvent(new PropertyChangeEvent(this, "layers", oldLayers, layers));
            }
        }
    }

    public void load(TileGrid tileGrid) throws IOException {
        setTileSet(tileGrid.getTileSet());
        setCellWidth(tileGrid.getCellWidth());
        List<Layer> layers = new ArrayList<Layer>(tileGrid.getLayersCount());
        for (Tile.TileLayer layerDDF : tileGrid.getLayersList()) {
            Layer layer = new Layer();
            layer.setId(layerDDF.getId());
            layer.setZ(layerDDF.getZ());
            layer.setVisible(layerDDF.getIsVisible() != 0);
            layers.add(layer);
        }
        setLayers(layers);
    }

    private static PropertyIntrospector<GridModel, GridModel> introspector = new PropertyIntrospector<GridModel, GridModel>(GridModel.class);

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<GridModel, GridModel>(this, this, introspector, null);
        }
        return null;
    }

}

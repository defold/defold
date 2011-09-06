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
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorSource;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileGrid;

public class GridModel extends Model implements IPropertyObjectWorld, IAdaptable {

    public class Cell implements IAdaptable {

        private PropertyIntrospectorSource<Cell, GridModel> propertySource;
        @Override
        public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
            if (adapter == IPropertySource.class) {
                if (this.propertySource == null) {
                    this.propertySource = new PropertyIntrospectorSource<Cell, GridModel>(this, GridModel.this, null);
                }
                return this.propertySource;
            }
            return null;
        }

        @Property(commandFactory = UndoableCommandFactory.class)
        private int tile;
        @Property(commandFactory = UndoableCommandFactory.class)
        private boolean hFlip;
        @Property(commandFactory = UndoableCommandFactory.class)
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

    public class Layer implements IAdaptable {

        private PropertyIntrospectorSource<Layer, GridModel> propertySource;
        @Override
        public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
            if (adapter == IPropertySource.class) {
                if (this.propertySource == null) {
                    this.propertySource = new PropertyIntrospectorSource<Layer, GridModel>(this, GridModel.this, null);
                }
                return this.propertySource;
            }
            return null;
        }

        @Property(commandFactory = UndoableCommandFactory.class)
        private String id;
        @Property(commandFactory = UndoableCommandFactory.class)
        private float z;
        @Property(commandFactory = UndoableCommandFactory.class)
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

    @Property(commandFactory = UndoableCommandFactory.class, isResource = true)
    private String tileSet;
    @Property(commandFactory = UndoableCommandFactory.class)
    private float cellWidth;
    @Property(commandFactory = UndoableCommandFactory.class)
    private float cellHeight;

    private List<Layer> layers;

    private final IOperationHistory history;
    private final IUndoContext undoContext;

    private IPropertySource propertySource;

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

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<GridModel, GridModel>(this, this, null);
            }
            return this.propertySource;
        }
        return null;
    }

}

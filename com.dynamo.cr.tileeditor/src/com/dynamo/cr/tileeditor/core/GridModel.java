package com.dynamo.cr.tileeditor.core;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorSource;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileGrid;

public class GridModel extends Model implements IPropertyObjectWorld, IOperationHistoryListener, IAdaptable {

    public static final int CHANGE_FLAG_PROPERTIES = (1 << 0);
    public static final int CHANGE_FLAG_LAYERS = (1 << 1);
    public static final int CHANGE_FLAG_CELLS = (1 << 2);

    public class Cell {
        @Property(commandFactory = UndoableCommandFactory.class)
        private int x;
        @Property(commandFactory = UndoableCommandFactory.class)
        private int y;
        @Property(commandFactory = UndoableCommandFactory.class)
        private int tile;
        @Property(commandFactory = UndoableCommandFactory.class)
        private boolean h_flip;
        @Property(commandFactory = UndoableCommandFactory.class)
        private boolean v_flip;
    }

    public class Layer {
        @Property(commandFactory = UndoableCommandFactory.class)
        private String id;
        @Property(commandFactory = UndoableCommandFactory.class)
        private float z;
        @Property(commandFactory = UndoableCommandFactory.class)
        private boolean visible;

        private final List<Cell> cells;

        public Layer() {
            cells = new ArrayList<Cell>();
        }

        public String getId() {
            return this.id;
        }

        public void setId(String id) {
            this.id = id;
        }

        public float getZ() {
            return this.z;
        }

        public void setZ(float z) {
            this.z = z;
        }

        public boolean isVisible() {
            return this.visible;
        }

        public void setVisible(boolean visible) {
            this.visible = visible;
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

    public float getCellWidth() {
        return this.cellWidth;
    }

    public float getCellHeight() {
        return this.cellHeight;
    }

    public List<Layer> getLayers() {
        return this.layers;
    }

    public void load(TileGrid tileGrid) throws IOException {
        this.tileSet = tileGrid.getTileSet();
        this.cellWidth = tileGrid.getCellWidth();
        this.cellHeight = tileGrid.getCellHeight();
        this.layers = new ArrayList<Layer>(tileGrid.getLayersCount());
        for (Tile.TileLayer layerDDF : tileGrid.getLayersList()) {
            Layer layer = new Layer();
            layer.setId(layerDDF.getId());
            layer.setZ(layerDDF.getZ());
            layer.setVisible(layerDDF.getIsVisible() != 0);
            this.layers.add(layer);
        }
        fireModelChangedEvent(new ModelChangedEvent(CHANGE_FLAG_PROPERTIES | CHANGE_FLAG_LAYERS | CHANGE_FLAG_CELLS));
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

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        // TODO Auto-generated method stub

    }

}

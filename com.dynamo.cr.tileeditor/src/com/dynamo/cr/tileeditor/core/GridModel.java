package com.dynamo.cr.tileeditor.core;

import java.beans.PropertyChangeEvent;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.inject.Inject;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.properties.Range;
import com.dynamo.tile.proto.Tile;
import com.dynamo.tile.proto.Tile.TileGrid;
import com.google.protobuf.TextFormat;

@Entity(commandFactory = GridUndoableCommandFactory.class)
public class GridModel extends Model implements ITileWorld, IAdaptable {

    public class Cell {

        private int tile;
        private boolean hFlip;
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
    public static class Layer implements IAdaptable {

        @Override
        public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
            if (adapter == IPropertyModel.class) {
                return new PropertyIntrospectorModel<Layer, GridModel>(this, gridModel, layerIntrospector);
            }
            return null;
        }

        @Property
        private String id;
        @Property
        private float z;
        @Property
        private boolean visible;

        GridModel gridModel;

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
                if (gridModel != null)
                    gridModel.firePropertyChangeEvent(new PropertyChangeEvent(this, "id", oldId, id));
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
    @Resource
    private String tileSet;
    @Property
    @Range(min=0)
    private float cellWidth;
    @Property
    @Range(min=0)
    private float cellHeight;

    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final ILogger logger;
    private final IContainer contentRoot;
    private final TileSetModel tileSetModel;

    private List<Layer> layers;

    @Inject
    public GridModel(IContainer contentRoot, IOperationHistory history, UndoContext undoContext, ILogger logger) {
        this.contentRoot = contentRoot;
        this.history = history;
        this.undoContext = undoContext;
        this.logger = logger;

        this.layers = new ArrayList<Layer>();
        this.tileSetModel = new TileSetModel(contentRoot, null, null);
    }

    @Override
    public IContainer getContentRoot() {
        return contentRoot;
    }

    public boolean isOk() {
        PropertyIntrospectorModel<GridModel, GridModel> propertyModel = new PropertyIntrospectorModel<GridModel, GridModel>(this, this, introspector);
        return propertyModel.isOk();
    }

    public String getTileSet() {
        return this.tileSet;
    }

    public void setTileSet(String tileSet) {
        if ((this.tileSet == null && tileSet != null) || !this.tileSet.equals(tileSet)) {
            String oldTileSet = this.tileSet;
            this.tileSet = tileSet;
            if (this.tileSet != null && !this.tileSet.equals("")) {
                IFile file = this.contentRoot.getFile(new Path(this.tileSet));
                try {
                    this.tileSetModel.load(file.getContents());
                } catch (Exception e) {
                    // TODO: Report error
                }
            }
            firePropertyChangeEvent(new PropertyChangeEvent(this, "tileSet", oldTileSet, tileSet));
        }
    }

    protected IStatus validateTileSet() {
        @SuppressWarnings("unchecked")
        IPropertyModel<TileSetModel, TileSetModel> propertyModel = (IPropertyModel<TileSetModel, TileSetModel>) this.tileSetModel.getAdapter(IPropertyModel.class);
        IStatus imageStatus = propertyModel.getPropertyStatus("image");
        if (imageStatus != null && !imageStatus.isOK()) {
            return new Status(IStatus.ERROR, "com.dynamo.cr.tileeditor", Messages.GRID_INVALID_TILESET);
        } else {
            return Status.OK_STATUS;
        }
    }

    public float getCellWidth() {
        return this.cellWidth;
    }

    public void setCellWidth(float cellWidth) {
        boolean fire = this.cellWidth != cellWidth;

        float oldCellWidth = this.cellWidth;
        this.cellWidth = cellWidth;
        if (fire)
            firePropertyChangeEvent(new PropertyChangeEvent(this, "cellWidth", new Float(oldCellWidth), new Float(cellWidth)));
    }

    public float getCellHeight() {
        return this.cellHeight;
    }

    public void setCellHeight(float cellHeight) {
        boolean fire = this.cellHeight != cellHeight;

        float oldCellHeight = this.cellHeight;
        this.cellHeight = cellHeight;
        if (fire)
            firePropertyChangeEvent(new PropertyChangeEvent(this, "cellHeight", new Float(oldCellHeight), new Float(cellHeight)));
    }

    public List<Layer> getLayers() {
        return new ArrayList<Layer>(this.layers);
    }

    public void setLayers(List<Layer> layers) {
        boolean fire = this.layers.equals(layers);

        List<Layer> oldLayers = this.layers;
        this.layers = new ArrayList<Layer>(layers);

        Set<String> idSet = new HashSet<String>();
        boolean duplication = false;
        for (Layer layer : this.layers) {
            layer.gridModel = this;
            if (idSet.contains(layer.getId())) {
                duplication = true;
            } else {
                idSet.add(layer.getId());
            }
        }

        if (duplication) {
            // TODO: Do something useful?
        }

        if (fire)
            firePropertyChangeEvent(new PropertyChangeEvent(this, "layers", oldLayers, layers));
    }

    public void load(InputStream is) {
        TileGrid.Builder tileGridBuilder = TileGrid.newBuilder();
        try {
            TextFormat.merge(new InputStreamReader(is), tileGridBuilder);
            TileGrid tileGrid = tileGridBuilder.build();
            setTileSet(tileGrid.getTileSet());
            setCellWidth(tileGrid.getCellWidth());
            setCellHeight(tileGrid.getCellHeight());
            List<Layer> layers = new ArrayList<Layer>(tileGrid.getLayersCount());
            for (Tile.TileLayer layerDDF : tileGrid.getLayersList()) {
                Layer layer = new Layer();
                layer.gridModel = this;
                layer.setId(layerDDF.getId());
                layer.setZ(layerDDF.getZ());
                layer.setVisible(layerDDF.getIsVisible() != 0);
                layers.add(layer);
            }
            setLayers(layers);
        } catch (IOException e) {
            logger.logException(e);
        }
    }

    private static PropertyIntrospector<GridModel, GridModel> introspector = new PropertyIntrospector<GridModel, GridModel>(GridModel.class, Messages.class);

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<GridModel, GridModel>(this, this, introspector);
        }
        return null;
    }

    public void executeOperation(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.history.execute(operation, null, null);
        } catch (final ExecutionException e) {
            this.logger.logException(e);
        }

        if (status != Status.OK_STATUS) {
            this.logger.logException(status.getException());
        }
    }

}

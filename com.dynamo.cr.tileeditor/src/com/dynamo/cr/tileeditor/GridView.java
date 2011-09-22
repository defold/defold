package com.dynamo.cr.tileeditor;

import java.awt.image.BufferedImage;
import java.util.List;
import java.util.Map;

import javax.inject.Inject;
import javax.vecmath.Point2f;

import org.eclipse.swt.graphics.Rectangle;

import com.dynamo.cr.tileeditor.core.IGridView;
import com.dynamo.cr.tileeditor.core.Layer;
import com.dynamo.cr.tileeditor.core.Layer.Cell;

public class GridView implements IGridView {

    @Inject private IGridView.Presenter presenter;

    @Override
    public void setTileSet(BufferedImage image, int tileWidth, int tileHeight,
            int tileMargin, int tileSpacing) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setCellWidth(float cellWidth) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setCellHeight(float cellHeight) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setLayers(List<Layer> layers) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setCells(int layerIndex, Map<Long, Cell> cells) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setCell(int layerIndex, int cellX, int cellY, int tileIndex,
            boolean hFlip, boolean vFlip) {
        // TODO Auto-generated method stub

    }

    @Override
    public void refreshProperties() {
        // TODO Auto-generated method stub

    }

    @Override
    public void setValidModel(boolean valid) {
        // TODO Auto-generated method stub

    }

    @Override
    public void setDirty(boolean dirty) {
        // TODO Auto-generated method stub

    }

    @Override
    public Rectangle getPreviewRect() {
        // TODO Auto-generated method stub
        return null;
    }

    @Override
    public void setPreview(Point2f position, float zoom) {
        // TODO Auto-generated method stub

    }

}

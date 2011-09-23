package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point2f;

import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.swt.graphics.Rectangle;

import com.dynamo.cr.tileeditor.core.Layer.Cell;

public interface IGridView {

    public interface Presenter {

        void onSelectTile(int tileIndex, boolean hFlip, boolean vFlip);

        void onPaintBegin();
        void onPaint(int x, int y);
        void onPaintEnd();

        void onLoad(InputStream is);

        void onSave(OutputStream os, IProgressMonitor monitor) throws IOException;

        void onPreviewPan(int dx, int dy);

        void onPreviewZoom(int delta);

        void onRefresh();

        boolean isDirty();
    }

    void setTileSet(BufferedImage image, int tileWidth, int tileHeight, int tileMargin, int tileSpacing);

    void setCellWidth(float cellWidth);

    void setCellHeight(float cellHeight);

    void setLayers(List<Layer> layers);

    void setCells(int layerIndex, Map<Long, Cell> cells);

    void setCell(int layerIndex, long cellIndex, Cell cell);

    void refreshProperties();

    void setValidModel(boolean valid);

    void setDirty(boolean dirty);

    Rectangle getPreviewRect();

    void setPreview(Point2f position, float zoom);

    void setSelectedTile(int tileIndex, boolean hFlip, boolean vFlip);

}

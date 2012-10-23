package com.dynamo.cr.tileeditor.core;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point2f;

import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.swt.graphics.Rectangle;

import com.dynamo.cr.tileeditor.core.Layer.Cell;

public interface IGridView {

    public interface Presenter {

        void onSelectTile(int tileIndex, boolean hFlip, boolean vFlip);

        void onSelectCells(Layer layer, int x0, int y0, int x1, int y1);

        void onAddLayer();
        void onSelectLayer(Layer layer);
        void onRemoveLayer();

        void onPaintBegin();
        void onPaint(int x, int y);
        void onPaintEnd();

        void onLoad(InputStream is);
        void onSave(OutputStream os, IProgressMonitor monitor) throws IOException;

        void onPreviewPan(int dx, int dy);
        void onPreviewZoom(double amount);
        void onPreviewFrame();
        void onPreviewResetZoom();

        void onRefresh();

        boolean isDirty();

        void onResourceChanged(IResourceChangeEvent e) throws CoreException, IOException;

    }

    void setTileSet(BufferedImage image, int tileWidth, int tileHeight, int tileMargin, int tileSpacing);

    void setLayers(List<Layer> layers);
    void setSelectedLayer(Layer layer);

    void setCells(int layerIndex, Map<Long, Cell> cells);

    void setCell(int layerIndex, long cellIndex, Cell cell);

    void refreshProperties();

    void setValidModel(boolean valid);

    void setDirty(boolean dirty);

    Rectangle getPreviewRect();

    void setPreview(Point2f position, float zoom);

    void setBrush(MapBrush brush);

}

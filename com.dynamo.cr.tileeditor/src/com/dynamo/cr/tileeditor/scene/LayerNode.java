package com.dynamo.cr.tileeditor.scene;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.cr.tileeditor.Activator;

@SuppressWarnings("serial")
public class LayerNode extends Node {

    @Property
    @NotEmpty
    @Unique
    private String id = "";

    @Property
    private float z = 0.0f;

    @Property
    private boolean visible = true;

    private Map<Long, Integer> cells = new HashMap<Long, Integer>();

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

    public Map<Long, Integer> getCells() {
        return Collections.unmodifiableMap(this.cells);
    }

    public void setCells(Map<Long, Integer> cells) {
        this.cells = cells;
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.LAYER_IMAGE_ID);
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

package com.dynamo.cr.tileeditor.core;

import com.dynamo.cr.tileeditor.core.Layer.Cell;


public class MapBrush {
	private final Cell[][] cells;

    public MapBrush() {
        this.cells = new Cell[1][1];
        this.cells[0][0] = new Cell(-1, false, false);
    }

    public MapBrush(int tileIndex, boolean hFlip, boolean vFlip) {
        this.cells = new Cell[1][1];
        this.cells[0][0] = new Cell(tileIndex, hFlip, vFlip);
    }

    public MapBrush(Cell[][] cells) {
    	this.cells = cells;
    }
    
    public int getWidth() {
    	return this.cells.length;
    }

    public int getHeight() {
    	return this.cells[0].length;
    }
    
    public Cell getCell(int x, int y) {
    	return this.cells[x][y];
    }
    
    @Override
    public boolean equals(Object obj) {
    	if (obj instanceof MapBrush) {
    		MapBrush brush = (MapBrush)obj;
    		int width = getWidth();
    		int height = getHeight();
    		if (width != brush.getWidth() || height != brush.getHeight()) {
    			return false;
    		}
    		for (int x = 0; x < width; ++x) {
    			for (int y = 0; y < height; ++y) {
    				if (!getCell(x, y).equals(brush.getCell(x, y))) {
    					return false;
    				}
    			}
    		}
    		return true;
    	}
    	return super.equals(obj);
    }
}

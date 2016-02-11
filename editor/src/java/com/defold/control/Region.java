package com.defold.control;

import javafx.scene.Scene;
import javafx.scene.Group;
import javafx.scene.Node;
import javafx.geometry.BoundingBox;
import javafx.geometry.Pos;
import javafx.geometry.VPos;
import javafx.geometry.HPos;

public class Region extends javafx.scene.layout.Region {
    public void layoutInArea(Node child, double areaX, double areaY, double areaWidth, double areaHeight, double areaBaselineOffset, HPos halignment, VPos valignment) {
	super.layoutInArea(child, areaX, areaY, areaWidth, areaHeight, areaBaselineOffset, halignment, valignment);
    }
    public void layoutChildren() {
	super.layoutChildren();
    }
}


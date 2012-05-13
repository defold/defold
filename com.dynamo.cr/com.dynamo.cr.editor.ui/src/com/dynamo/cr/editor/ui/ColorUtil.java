package com.dynamo.cr.editor.ui;

import java.awt.Color;

public class ColorUtil {
    public static Color createColorFromHueAlpha(float hue, float alpha) {
        float r = 0.0f, g = 0.0f, b = 0.0f;
        float h_p = hue / 60.0f;
        float c = 1.0f;
        float x = c * (1.0f - Math.abs((h_p % 2.0f) - 1.0f));
        int type = (int)h_p;
        switch (type) {
        case 0: r = c; g = x; break;
        case 1: r = x; g = c; break;
        case 2: g = c; b = x; break;
        case 3: g = x; b = c; break;
        case 4: r = x; b = c; break;
        case 5: r = c; b = x; break;
        }
        return new Color(r, g, b, alpha);
    }
}

package com.dynamo.cr.sceneed.core;

import java.util.Arrays;

import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

public class SceneGrid {
    // Sizes of the grids, both are only used when the grid is set to auto
    double[] gridSizes = new double[2];
    // Ratio of the first grid compared to the other, only used for auto grids
    double firstGridRatio = 1.0f;
    // AABBs of the grids, capped by the current view frustum
    AABB[] aabbs = new AABB[2];

    boolean autoGrid;
    double fixedGridSize;
    float[] gridColor;

    public SceneGrid() {
        this.aabbs[0] = new AABB();
        this.aabbs[1] = new AABB();
    }

    public double getPrimarySize() {
        if (this.firstGridRatio >= 0.5f) {
            return this.gridSizes[0];
        } else {
            return this.gridSizes[1];
        }
    }

    public double getSecondarySize() {
        if (this.firstGridRatio < 0.5f) {
            return this.gridSizes[0];
        } else {
            return this.gridSizes[1];
        }
    }

    public double getPrimaryRatio() {
        if (this.firstGridRatio >= 0.5f) {
            return this.firstGridRatio;
        } else {
            return 1.0f - this.firstGridRatio;
        }
    }

    public AABB[] getAABBs() {
        return this.aabbs;
    }

    public boolean isAutoGrid() {
        return this.autoGrid;
    }

    public void setAutoGrid(boolean autoGrid) {
        this.autoGrid = autoGrid;
    }

    public void setFixedGridSize(double fixedGridSize) {
        this.fixedGridSize = fixedGridSize;
    }

    public float[] getGridColor() {
        return this.gridColor;
    }

    public void setGridColor(float[] gridColor) {
        this.gridColor = gridColor;
    }

    public void updateGrids(Matrix4d view, Matrix4d projection) {

        this.aabbs[0].setIdentity();
        this.aabbs[1].setIdentity();

        // Calculate frustum planes of the viewproj
        Matrix4d viewProj = new Matrix4d(projection);
        viewProj.mul(view);
        Vector4d p = new Vector4d();
        viewProj.getRow(3, p);
        Vector4d n = new Vector4d();
        Vector4d[] frustumPlanes = new Vector4d[6];
        for (int i = 0; i < 3; ++i) {
            for (int s = 0; s < 2; ++s) {
                viewProj.getRow(i, n);
                n.scale(Math.signum(s - 0.5));
                n.add(p);
                frustumPlanes[i * 2 + s] = new Vector4d(n);
            }
        }
        // Find the axis which is most aligned to the local z of the view proj
        Vector4d axis = new Vector4d();
        double[] values = new double[4];
        int perpAxis = 0;
        double maxAlign = 0.0f;
        for (int i = 0; i < 3; ++i) {
            Arrays.fill(values, 0.0);
            values[i] = 1.0;
            axis.set(values);
            n.set(frustumPlanes[5]);
            n.w = 0.0;
            n.normalize();
            double align = Math.abs(axis.dot(n));
            if (align > maxAlign) {
                maxAlign = align;
                perpAxis = i;
            }
        }

        Matrix3d m = new Matrix3d();
        Vector3d v = new Vector3d();

        int[] planeIndices = new int[] {
                0, 2,
                0, 3,
                1, 2,
                1, 3
        };

        AABB aabb = new AABB();
        for (int j = 0; j < 4; ++j) {
            m.setRow(0, axis.x, axis.y, axis.z);
            v.x = 0.0;

            p.set(frustumPlanes[planeIndices[j * 2 + 0]]);
            m.setRow(1, p.x, p.y, p.z);
            v.y = -p.w;

            p.set(frustumPlanes[planeIndices[j * 2 + 1]]);
            m.setRow(2, p.x, p.y, p.z);
            v.z = -p.w;

            m.invert();
            m.transform(v);

            aabb.union(v.x, v.y, v.z);
        }

        updateGridSizes(aabb, perpAxis);

        double[] minValues = new double[3];
        double[] maxValues = new double[3];

        int gridCount = 1;
        if (this.firstGridRatio < 1.0) {
            gridCount = 2;
        }
        // Clamp AABBs
        for (int i = 0; i < gridCount; ++i) {
            aabb.getMin().get(minValues);
            aabb.getMax().get(maxValues);
            double gridSize = this.gridSizes[i] * 10;
            for (int k = 0; k < 3; ++k) {
                minValues[k] = Math.floor(minValues[k] / gridSize) * gridSize;
                maxValues[k] = Math.ceil(maxValues[k] / gridSize) * gridSize;
            }

            this.aabbs[i].union(minValues[0], minValues[1], minValues[2]);
            this.aabbs[i].union(maxValues[0], maxValues[1], maxValues[2]);
        }
    }

    private void updateGridSizes(AABB aabb, int perpAxis) {
        if (!this.autoGrid) {
            this.gridSizes[0] = this.fixedGridSize;
            this.gridSizes[1] = 0.0;
            this.firstGridRatio = 1.0;
        } else {
            double[] dimValues = new double[3];
            Vector3d dims = new Vector3d(aabb.getMax());
            dims.sub(aabb.getMin());
            dims.get(dimValues);
            double min = Double.POSITIVE_INFINITY;
            // Find dimensions of the grid
            for (int i = 0; i < 3; ++i) {
                if (perpAxis != i) {
                    min = Math.min(min, dimValues[i]);
                }
            }
            double exp = Math.log10(min);
            double e = Math.floor(exp);
            this.firstGridRatio = 1.0 - (exp - e);
            for (int i = 0; i < 2; ++i) {
                this.gridSizes[i] = Math.pow(10.0, e + i - 1);
            }
        }
    }
}

package com.dynamo.cr.sceneed.ui;

import javax.media.opengl.GL;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;

import com.dynamo.cr.sceneed.core.AABB;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.core.SceneGrid;

public class GridRenderer implements INodeRenderer<GridNode> {

    public GridRenderer() {
    }

    @Override
    public void dispose(GL gl) { }

    @Override
    public void setup(RenderContext renderContext, GridNode node) {
        if (renderContext.getPass() == Pass.TRANSPARENT) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, GridNode node,
            RenderData<GridNode> renderData) {
        SceneGrid grid = node.getGrid();

        GL gl = renderContext.getGL();

        double[] gridSizes = new double[2];
        gridSizes[0] = grid.getPrimarySize();
        gridSizes[1] = grid.getSecondarySize();
        double[] gridRatios = new double[2];
        gridRatios[0] = grid.getPrimaryRatio();
        gridRatios[1] = 1.0 - gridRatios[0];
        AABB[] aabbs = grid.getAABBs();
        float[] gridColor = grid.getGridColor();

        gl.glBegin(GL.GL_LINES);
        final double minAlign = 0.7071;
        Matrix4d view = new Matrix4d(renderContext.getRenderView().getViewTransform());
        double[] dir = new double[4];
        view.getRow(2, dir);
        for (int gridIndex = 0; gridIndex < 2; ++gridIndex) {
            if (!aabbs[gridIndex].isIdentity()) {
                for (int axis = 0; axis < 3; ++axis) {
                    if (dir[axis] > minAlign) {
                        double alpha = dir[axis] * gridRatios[gridIndex];
                        gl.glColor4d(gridColor[0], gridColor[1], gridColor[2], alpha);
                        if (gridRatios[gridIndex] > 0.0) {
                            renderGrid(gl, axis, gridSizes[gridIndex], aabbs[gridIndex]);
                        }
                    }
                }
            }
        }

        AABB aabb = new AABB();
        aabb.union(aabbs[0]);
        aabb.union(aabbs[1]);
        // Render principal axis in RGB
        gl.glColor4d(1.0, 0.0, 0.0, 1.0);
        gl.glVertex3d(aabb.getMin().getX(), 0.0, 0.0);
        gl.glVertex3d(aabb.getMax().getX(), 0.0, 0.0);
        gl.glColor4d(0.0, 1.0, 0.0, 1.0);
        gl.glVertex3d(0.0, aabb.getMin().getY(), 0.0);
        gl.glVertex3d(0.0, aabb.getMax().getY(), 0.0);
        gl.glColor4d(0.0, 0.0, 1.0, 1.0);
        gl.glVertex3d(0.0, 0.0, aabb.getMin().getZ());
        gl.glVertex3d(0.0, 0.0, aabb.getMax().getZ());

        gl.glEnd();
    }

    private void renderGrid(GL gl, int axis, double size, AABB aabb) {
        double[] minValues = new double[4];
        aabb.getMin().get(minValues);
        double[] maxValues = new double[4];
        aabb.getMax().get(maxValues);

        double[] vertex = new double[3];
        int axis1 = axis;
        int axis2 = (axis + 1) % 3;
        int axis3 = (axis + 2) % 3;
        for (int k = 0; k < 2; ++k) {
            for (double v = minValues[axis2]; v <= maxValues[axis2]; v += size) {
                vertex[axis1] = 0.0;
                vertex[axis2] = v;
                vertex[axis3] = minValues[axis3];
                gl.glVertex3dv(vertex, 0);
                vertex[axis3] = maxValues[axis3];
                gl.glVertex3dv(vertex, 0);
            }
            int tmp = axis2;
            axis2 = axis3;
            axis3 = tmp;
        }
    }

}

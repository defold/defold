package com.dynamo.cr.sceneed.ui;

import java.util.Arrays;

import javax.media.opengl.GL;
import javax.vecmath.Matrix3d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import org.eclipse.jface.preference.IPreferenceStore;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.INodeRenderer;
import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.RenderContext.Pass;
import com.dynamo.cr.sceneed.core.RenderData;
import com.dynamo.cr.sceneed.ui.preferences.PreferenceConstants;

public class GridRenderer implements INodeRenderer<GridNode> {

    public GridRenderer() {
    }

    @Override
    public void setup(RenderContext renderContext, GridNode node) {
        if (renderContext.getPass() == Pass.TRANSPARENT) {
            renderContext.add(this, node, new Point3d(), null);
        }
    }

    @Override
    public void render(RenderContext renderContext, GridNode node,
            RenderData<GridNode> renderData) {
        GL gl = renderContext.getGL();
        float[] gridColor = RenderUtil.parseColor(PreferenceConstants.P_GRID_COLOR);

        IPreferenceStore store = Activator.getDefault().getPreferenceStore();
        boolean autoGrid = store.getString(PreferenceConstants.P_GRID).equals(PreferenceConstants.P_GRID_AUTO_VALUE);
        double gridSize = store.getInt(PreferenceConstants.P_GRID_SIZE);

//        gl.glDisable(GL.GL_LIGHTING);
//        gl.glEnable(GL.GL_BLEND);
//        gl.glBlendFunc(GL.GL_SRC_ALPHA, GL.GL_ONE_MINUS_SRC_ALPHA);
//        gl.glPolygonMode(GL.GL_FRONT_AND_BACK, GL.GL_LINE);

        gl.glBegin(GL.GL_LINES);

        Matrix4d viewProj = new Matrix4d(renderContext.getRenderView().getProjectionTransform());
        Matrix4d view = new Matrix4d(renderContext.getRenderView().getViewTransform());
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
        Vector4d axis = new Vector4d();
        double[] values = new double[4];
        for (int i = 0; i < 3; ++i) {
            Arrays.fill(values, 0.0);
            values[i] = 1.0;
            axis.set(values);
            double min_align = 0.7071;
            n.set(frustumPlanes[5]);
            n.w = 0.0;
            n.normalize();
            double align = Math.abs(axis.dot(n));
            if (align > min_align) {
                Arrays.fill(values, Double.POSITIVE_INFINITY);
                Vector4d aabbMin = new Vector4d(values);
                aabbMin.w = 1.0;
                Arrays.fill(values, Double.NEGATIVE_INFINITY);
                Vector4d aabbMax = new Vector4d(values);
                aabbMax.w = 1.0;

                Matrix3d m = new Matrix3d();
                Vector3d v = new Vector3d();

                int[] planeIndices = new int[] {
                        0, 2,
                        0, 3,
                        1, 2,
                        1, 3
                };

                for (int j = 0; j < 4; ++j) {
                    m.setRow(0, axis.x, axis.y, axis.z);
                    v.x = 0.0;

                    p.set(frustumPlanes[planeIndices[j*2 + 0]]);
                    m.setRow(1, p.x, p.y, p.z);
                    v.y = -p.w;

                    p.set(frustumPlanes[planeIndices[j*2 + 1]]);
                    m.setRow(2, p.x, p.y, p.z);
                    v.z = -p.w;

                    m.invert();
                    m.transform(v);

                    aabbMin.x = Math.min(aabbMin.x, v.x);
                    aabbMin.y = Math.min(aabbMin.y, v.y);
                    aabbMin.z = Math.min(aabbMin.z, v.z);

                    aabbMax.x = Math.max(aabbMax.x, v.x);
                    aabbMax.y = Math.max(aabbMax.y, v.y);
                    aabbMax.z = Math.max(aabbMax.z, v.z);
                }

                Vector4d aabbDims = new Vector4d(aabbMax);
                aabbDims.sub(aabbMin);
                aabbDims.get(values);
                double exp = Double.POSITIVE_INFINITY;
                for (int j = 0; j < 3; ++j) {
                    if (i != j) {
                        exp = Math.min(exp, Math.log10(values[j]));
                    }
                }
                double e = Math.floor(exp);
                int gridSizes = 1;
                if (autoGrid) {
                    gridSizes = 2;
                }
                double maxAlpha = 0.0;
                for (int j = 0; j < gridSizes; ++j) {
                    double alpha = (align - min_align)/(1.0 - min_align);
                    if (autoGrid) {
                        alpha *= (1.0 - Math.abs(exp - e));
                    }
                    gl.glColor4d(gridColor[0], gridColor[1], gridColor[2], alpha);

                    double base = gridSize;
                    if (autoGrid) {
                        base = Math.pow(10.0, e);
                        if (alpha > maxAlpha) {
                            gridSize = base * 0.1;
                            maxAlpha = alpha;
                        }
                    }

                    double[] minValues = new double[4];
                    aabbMin.get(minValues);
                    double[] maxValues = new double[4];
                    aabbMax.get(maxValues);

                    for (int k = 0; k < 3; ++k) {
                        minValues[k] = Math.floor(minValues[k]/base) * base;
                        maxValues[k] = Math.ceil(maxValues[k]/base) * base;
                    }

                    double step = 0.1*gridSize;
                    if (autoGrid) {
                        step = Math.pow(10.0, e-1);
                    }
                    double[] vertex = new double[3];
                    int axis1 = i;
                    int axis2 = (axis1 + 1) % 3;
                    int axis3 = (axis1 + 2) % 3;
                    for (int k = 0; k < 2; ++k) {
                        for (double dv = minValues[axis2]; dv <= maxValues[axis2]; dv += step) {
                            vertex[axis1] = 0.0;
                            vertex[axis2] = dv;
                            vertex[axis3] = minValues[axis3];
                            gl.glVertex3dv(vertex, 0);
                            vertex[axis3] = maxValues[axis3];
                            gl.glVertex3dv(vertex, 0);
                        }
                        int tmp = axis2;
                        axis2 = axis3;
                        axis3 = tmp;
                    }

                    e += 1.0;
                }
            }
        }

        gl.glEnd();
    }

}

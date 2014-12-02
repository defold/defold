package com.dynamo.cr.sceneed.core;

import java.util.ArrayList;
import java.util.Collections;
import java.util.EnumSet;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.media.opengl.GL2;
import javax.media.opengl.glu.GLU;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;

import org.eclipse.jface.viewers.ISelection;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.sceneed.ui.TextureRegistry;
import com.jogamp.opengl.util.awt.TextRenderer;

public class RenderContext {
    private GL2 gl;
    private GLU glu;
    private Pass pass;
    private ArrayList<RenderData<? extends Node>> renderDataList;
    private Set<Node> selectedNodes = new HashSet<Node>();
    private IRenderView renderView;
    private TextureRegistry textureRegistry;
    private double dt;
    private TextRenderer smallTextRenderer;

    public enum Pass {
        /**
         * Background overlay pass
         */
        BACKGROUND(false, false),

        /**
         * Opaque pass
         */
        OPAQUE(false, true),

        /**
         * Transparent pass
         */
        TRANSPARENT(false, true),

        /**
         * Icon outline pass
         */
        ICON_OUTLINE(false, false),

        /**
         * Outline pass
         */
        OUTLINE(false, true),

        /**
         * Manipulator pass
         */
        MANIPULATOR(false, true),

        /**
         * Overlay pass for marquee-box and such
         */
        OVERLAY(false, false),

        /**
         * Generic selection pass
         */
        SELECTION(true, true),

        /**
         * Icon overlay pass
         */
        ICON(false, false),

        /**
         * Icon overlay selection pass
         */
        ICON_SELECTION(true, false);

        private final boolean isSelectionPass;
        private final boolean transformModel;

        Pass(boolean isSelectionPass, boolean transformModel) {
            this.isSelectionPass = isSelectionPass;
            this.transformModel = transformModel;
        }

        public static Pass[] getSelectionPasses() {
            return new Pass[] { Pass.SELECTION, Pass.ICON_SELECTION };
        }

        /**
         * Is the pass a selection render pass
         * @return true if the pass is a selection render pass
         */
        public boolean isSelectionPass() {
            return isSelectionPass;
        }

        /**
         * Should the current model-view-matrix be transformed
         * according to node transform.
         * This is typically true but false overlay-renderers and such
         * that do the complete transformation
         *
         * @return true if the model-view should be transformed
         */
        public boolean transformModel() {
            return transformModel;
        }
    }

    public enum Status {
        SELECTED,
        NONE,
    }

    public enum Color {
        CONVEX_SHAPE,
    }

    @SuppressWarnings("unchecked")
    public RenderContext(IRenderView renderView, double dt, GL2 gl, GLU glu, TextureRegistry textureRegistry, ISelection selection, TextRenderer smallTextRenderer) {
        this.renderView = renderView;
        this.dt = dt;
        this.gl = gl;
        this.glu = glu;
        this.textureRegistry = textureRegistry;
        this.smallTextRenderer = smallTextRenderer;
        this.renderDataList = new ArrayList<RenderData<? extends Node>>(1024);
        if (selection instanceof IStructuredSelection) {
            IStructuredSelection structSel = (IStructuredSelection) selection;
            this.selectedNodes.addAll(structSel.toList());
        }
    }

    public TextureRegistry getTextureRegistry() {
        return textureRegistry;
    }

    public IRenderView getRenderView() {
        return renderView;
    }

    public GL2 getGL() {
        return gl;
    }

    public GLU getGLU() {
        return glu;
    }

    public Pass getPass() {
        return pass;
    }

    public void setPass(Pass pass) {
        this.pass = pass;
    }

    public boolean shouldDraw(EnumSet<Pass> passes) {
        return passes.contains(pass);
    }

    public <T extends Node> RenderData<T> add(INodeRenderer<T> nodeRenderer, T node, Point3d position, Object userData) {
        RenderData<T> data = new RenderData<T>(pass, nodeRenderer, node, position, userData);
        renderDataList.add(data);
        return data;
    }

    public List<RenderData<? extends Node>> getRenderData() {
        return renderDataList;
    }

    public void sort() {
        Camera camera = this.renderView.getCamera();
        Matrix4d m = new Matrix4d();
        Point3d p = new Point3d();
        for (RenderData<? extends Node> renderData : renderDataList) {
            renderData.calculateKey(camera, m, p);
        }

        Collections.sort(renderDataList);
    }

    // NOTE: We hard-code colors for now
    private static float OBJECT_COLOR[] = new float[] { 43.0f/255, 25.0f/255, 116.0f/255 };
    private static float SELECTED_COLOR[] = new float[] { 69.0f/255, 255.0f/255, 162.0f/255 };

    public boolean isSelected(Node node) {
        Node n = node;
        while (n != null && n.getParent() != null && !selectedNodes.contains(n)) {
            n = n.getParent();
        }
        return  (n != null && n.getParent() != null);
    }

    /**
     * Select color based on pass. If not default color is selected objectColor is returned
     * @param node node to render
     * @param objectColor object color to fallback to
     * @return color as float[4]
     */
    public float[] selectColor(Node node, float[] objectColor) {
        switch (pass) {
        case OPAQUE:
            return objectColor;
        case ICON_OUTLINE:
        case OUTLINE:
            if (isSelected(node)) {
                return SELECTED_COLOR;
            } else {
                return OBJECT_COLOR;
            }
        case TRANSPARENT:
            return objectColor;
        }

        return objectColor;
    }

    public double getDt() {
        return dt;
    }

    public TextRenderer getSmallTextRenderer() {
        return smallTextRenderer;
    }

}

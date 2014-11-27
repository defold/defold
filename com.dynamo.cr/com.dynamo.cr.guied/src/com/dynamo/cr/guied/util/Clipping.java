package com.dynamo.cr.guied.util;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point3d;
import javax.vecmath.Vector3d;
import javax.vecmath.Vector4d;

import com.dynamo.cr.sceneed.core.RenderContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.ui.RenderUtil;

import com.dynamo.cr.guied.core.BoxNode;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.gui.proto.Gui.NodeDesc.ClippingMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Pivot;

public class Clipping {
    private static boolean colorMaskEnabled = true;
    public static boolean showClippingNodesEnabled = true;

    public static void showClippingNodes(boolean show) {
        showClippingNodesEnabled = show;
    }

    public static boolean getShowClippingNodes() {
        return showClippingNodesEnabled;
    }

    public static class ClippingState {
        public int stencilRefVal;
        public int stencilMask;
        public int stencilParentMask;
        public int scissorX, scissorY;
        public int scissorWidth, scissorHeight;
        public boolean inverted;
    }

    public static void setState(RenderContext renderContext,  GuiNode node) {
        if(!renderContext.getPass().isClippingEnabled()) {
            return;
        }
        ScissorClipping.setState(renderContext, node);
        StencilClipping.setState(renderContext, node);
    }

    public static void setOutLineState(RenderContext renderContext,  GuiNode node) {
        ClippingMode mode = node.getClippingMode();
        if(mode == ClippingMode.CLIPPING_MODE_NONE) {
            return;
        }
        GL2 gl = renderContext.getGL();
        switch(mode) {
        case CLIPPING_MODE_SCISSOR:
            gl.glLineStipple(1, (short)0xFCFC);
            gl.glEnable(GL2.GL_LINE_STIPPLE);
            RenderUtil.loadMatrix(gl, Clipping.getTranslationScaleTransform(renderContext, node));
            break;

        case CLIPPING_MODE_STENCIL:
            gl.glLineStipple(2, (short)0xAAAA);
            gl.glEnable(GL2.GL_LINE_STIPPLE);
            break;

        default:
            break;
        }
    }

    public enum ValidationResult {
        OK,
        INCOMPATIBLE_NODE_TYPE,
        STENCIL_HIERARCHY_EXCEEDED,
    }

    public static ValidationResult validate(GuiNode node) {
        switch(node.getClippingMode()) {
        case CLIPPING_MODE_SCISSOR:
            return ScissorClipping.validate(node);

        case CLIPPING_MODE_STENCIL:
            return StencilClipping.validate(node);

        default:
            break;
        }
        return ValidationResult.OK;
    }


    private static Matrix4d getTranslationScaleTransform(RenderContext renderContext, GuiNode node) {
        // SVD translation and scale
        Matrix4d transform = new Matrix4d();
        node.getWorldTransform(transform);
        Vector3d tv = new Vector3d();
        tv.set(transform.getM00(), transform.getM10(), transform.getM20());
        double sX = tv.length();
        tv.set(transform.getM01(), transform.getM11(), transform.getM21());
        double sY = tv.length();
        Vector4d pos = new Vector4d(transform.getM03(), transform.getM13(), 0.0, 1.0);
        renderContext.getRenderView().getViewTransform().transform(pos);

        // Compose new matrix with only translation and scale
        transform.setIdentity();
        transform.setM03(pos.x);
        transform.setM13(pos.y);
        transform.setM00(sX);
        transform.setM11(sY);
        transform.setM22(1.0);
        return transform;
    }

    private static class ScissorClipping {

        private static GuiNode getParentScissor(GuiNode node) {
            Node parent = node.getParent();
            if((parent == null) || (!(parent instanceof GuiNode))) {
                return null;
            }
            GuiNode parentGuiNode = (GuiNode) parent;
            if(parentGuiNode.getClippingMode() == ClippingMode.CLIPPING_MODE_SCISSOR) {
                return parentGuiNode;
            }
            return getParentScissor(parentGuiNode);
        }

        private static void calculateState(GuiNode node, RenderContext renderContext, int x0, int y0, int x1, int y1) {
            if (node.getClippingMode() == ClippingMode.CLIPPING_MODE_SCISSOR ) {

                // SVD decomposition of transform, to set up scissor rectangle without rotation
                Matrix4d transform = new Matrix4d();
                node.getWorldTransform(transform);
                double[] dCol = new double[4];
                Vector3d vCol = new Vector3d();
                transform.getColumn(0, dCol);
                vCol.set(dCol);
                double width = node.getSize().x*vCol.length();
                transform.getColumn(1, dCol);
                vCol.set(dCol);
                double height = node.getSize().y*vCol.length();

                double xPos, yPos;
                Pivot p = node.getPivot();
                switch (p) {
                    case PIVOT_CENTER:
                    case PIVOT_S:
                    case PIVOT_N:
                        xPos = width*0.5;
                        break;
                    case PIVOT_SW:
                    case PIVOT_W:
                    case PIVOT_NW:
                        xPos = 0.0;
                        break;
                    default:
                        xPos = width;
                        break;
                }
                switch (p) {
                    case PIVOT_CENTER:
                    case PIVOT_E:
                    case PIVOT_W:
                        yPos = height*0.5;
                        break;
                    case PIVOT_S:
                    case PIVOT_SW:
                    case PIVOT_SE:
                        yPos = 0.0;
                        break;
                    default:
                        yPos = height;
                        break;
                }

                // transform into screen space (used by HW scissors)
                double wX = (transform.getM03() - xPos);
                double wY = (transform.getM13() - yPos);
                double[] pBL = renderContext.getRenderView().worldToScreen(new Point3d(wX, wY, 0.0));
                double[] pTR = renderContext.getRenderView().worldToScreen(new Point3d(wX + width, wY + height, 0.0));

                ClippingState state = node.getClippingState();
                state.scissorX = x0 = Math.max(x0, (int)pBL[0]);
                state.scissorY = y0 = Math.max(y0, (int)pBL[1]);
                x1 = Math.min(x1, (int)pTR[0]);
                y1 = Math.min(y1, (int)pTR[1]);
                state.scissorWidth = Math.max(0, x1 - x0);
                state.scissorHeight = Math.max(0, y1 - y0);
                state.inverted = false;
            }

            for (Node child : node.getChildren()) {
                calculateState((GuiNode)child, renderContext, x0, y0, x1, y1);
            }
        }

        private static void setState(RenderContext renderContext,  GuiNode node) {
            GL2 gl = renderContext.getGL();
            if(!colorMaskEnabled) {
                colorMaskEnabled = true;
                gl.glColorMask(true, true, true, true);
            }

            GuiNode scissorNode = getParentScissor(node);
            if(node.getClippingMode() == ClippingMode.CLIPPING_MODE_SCISSOR) {
                if (!node.getClippingVisible()) {
                    // set color mask depending on visibility
                    if(!node.getClippingVisible()) {
                        gl.glColorMask(false, false, false, false);
                        colorMaskEnabled = false;
                    }
                }
                if(scissorNode == null) {
                    // primary scissor, setup child scissors hierarchy states (per frame caching)
                    calculateState(node, renderContext, -32767, -32767, 32767, 32767);
                    scissorNode = node;
                }
                // scissor nodes cancels out rotation
                RenderUtil.loadMatrix(gl, getTranslationScaleTransform(renderContext, node));
            }

            if(scissorNode == null)
                return;
            ClippingState state = scissorNode.getClippingState();
            gl.glScissor(state.scissorX, state.scissorY, state.scissorWidth, state.scissorHeight);
            gl.glEnable(GL2.GL_SCISSOR_TEST);
        }

        private static ValidationResult validate(GuiNode node) {
            if(!(node instanceof BoxNode)) {
                return ValidationResult.INCOMPATIBLE_NODE_TYPE;
            }
            return ValidationResult.OK;
        }

    }

    private static class StencilClipping {

        private static boolean hasParentStencil(GuiNode node) {
            Node parent = node.getParent();
            if((parent == null) || (!(parent instanceof GuiNode))) {
                return false;
            }
            GuiNode parentGuiNode = (GuiNode) parent;
            if(parentGuiNode.getClippingMode() == ClippingMode.CLIPPING_MODE_STENCIL) {
                return true;
            }
            return hasParentStencil(parentGuiNode);
        }

        private static int countChildSiblingStencils(GuiNode node) {
            int stencilCount = 0;
            for (Node child : node.getChildren()) {
                GuiNode guiChild = (GuiNode) child;
                if(guiChild.getClippingMode() == ClippingMode.CLIPPING_MODE_STENCIL) {
                    ++stencilCount;
                    continue;
                }
                stencilCount += countChildSiblingStencils(guiChild);
            }
            return stencilCount;
        }

        private static void calculateState(GuiNode node, int bitRange, int refVal, int mask, int parentMask, boolean clippingInverted) {
            ClippingState state = node.getClippingState();
            state.stencilRefVal = refVal;
            state.stencilMask = mask;
            state.stencilParentMask = parentMask;
            state.inverted = clippingInverted;

            int childBitRange = 0;
            if (node.getClippingMode() == ClippingMode.CLIPPING_MODE_STENCIL ) {
                // set inverted mode
                clippingInverted = node.getClippingInverted();
                // calculate how many stencils there are in this tier and how many bits needed to represent them
                int childMaskBits = 0;
                int shift = countChildSiblingStencils(node);
                while (shift > 0) {
                    ++childBitRange;
                    childMaskBits |= shift;
                    shift = shift >> 1;
                }
                childBitRange += bitRange;
                mask |= (childMaskBits << bitRange);
            }
            // recursively visit the nodes of the child hierarchy, enumerating the stencils by number in tier
            refVal = 0;
            for (Node child : node.getChildren()) {
                GuiNode guiChild = (GuiNode) child;
                if(guiChild.getClippingMode() == ClippingMode.CLIPPING_MODE_STENCIL) {
                    refVal++;
                    calculateState(guiChild, childBitRange, (refVal << bitRange) | state.stencilRefVal, mask, state.stencilMask, clippingInverted);
                } else {
                    calculateState(guiChild, bitRange, state.stencilRefVal, state.stencilRefVal, state.stencilMask, clippingInverted);
                }
            }
        }

        private static void setState(RenderContext renderContext,  GuiNode node) {
            GL2 gl = renderContext.getGL();
            if(!colorMaskEnabled) {
                colorMaskEnabled = true;
                gl.glColorMask(true, true, true, true);
            }

            boolean isStencilChild = hasParentStencil(node);
            if(node.getClippingMode() == ClippingMode.CLIPPING_MODE_STENCIL) {
                // set color mask depending on visibility
                if(!node.getClippingVisible()) {
                    gl.glColorMask(false, false, false, false);
                    colorMaskEnabled = false;
                }
                gl.glEnable(GL2.GL_STENCIL_TEST);
                gl.glStencilMask(0xFF);
                if(!isStencilChild) {
                    // primary stencil, draw primary stencil unconditionally after clearing stencil buffer
                    calculateState(node, 1, 1, 1, 0, false); // setup child stencils hierarchy states (per frame caching)
                    gl.glClearStencil(0);
                    gl.glClear(GL.GL_STENCIL_BUFFER_BIT);
                    gl.glStencilFunc(GL2.GL_ALWAYS, 1, 0xff);
                    gl.glStencilOp(GL2.GL_KEEP, GL2.GL_REPLACE, GL2.GL_REPLACE);
                } else {
                    // sub-stencil, draw stencil masked by parent stencil
                    ClippingState state = node.getClippingState();
                    if(state.stencilMask < 256) {
                        gl.glStencilFunc(state.inverted ? GL2.GL_NOTEQUAL : GL2.GL_EQUAL, state.stencilRefVal, state.stencilParentMask);
                        gl.glStencilOp(GL2.GL_KEEP, GL2.GL_REPLACE, GL2.GL_REPLACE);
                    }
                }
            } else if (isStencilChild) {
                // not a stencil, but masked by a parent stencil
                ClippingState state = node.getClippingState();
                if(state.stencilMask < 256) {
                    gl.glEnable(GL2.GL_STENCIL_TEST);
                    gl.glStencilMask(0x00);
                    gl.glStencilFunc(state.inverted ? GL2.GL_NOTEQUAL : GL2.GL_EQUAL, state.stencilRefVal, state.stencilMask);
                }
            }
        }

        private static int calculateHierarchyMask(GuiNode node, int bitRange, int refVal, int mask) {
            int childBitRange = 0;
            if (node.getClippingMode() == ClippingMode.CLIPPING_MODE_STENCIL ) {
                // calculate how many stencils there are in this tier and how many bits needed to represent them
                int childMaskBits = 0;
                int shift = countChildSiblingStencils(node);
                while (shift > 0) {
                    ++childBitRange;
                    childMaskBits |= shift;
                    shift = shift >> 1;
                }
                childBitRange += bitRange;
                mask |= (childMaskBits << bitRange);
            }

            // recursively visit the nodes of the child hierarchy, complementing range-mask with current
            int parentRefVal = refVal;
            int childMask = mask;
            refVal = 0;
            for (Node child : node.getChildren()) {
                GuiNode guiChild = (GuiNode) child;
                if(guiChild.getClippingMode() == ClippingMode.CLIPPING_MODE_STENCIL) {
                    refVal++;
                    childMask |= calculateHierarchyMask(guiChild, childBitRange, (refVal << bitRange) | parentRefVal, mask);
                } else {
                    childMask |= calculateHierarchyMask(guiChild, childBitRange, parentRefVal, mask);
                }
            }
            return childMask;
        }

        private static ValidationResult validate(GuiNode node) {
            GuiNode primaryStencil = node;
            Node parentNode = node.getParent();
            while(parentNode != null && (parentNode instanceof GuiNode)) {
                if(((GuiNode)parentNode).getClippingMode()==ClippingMode.CLIPPING_MODE_STENCIL) {
                    primaryStencil = (GuiNode) parentNode;
                }
                parentNode = parentNode.getParent();
            }
            int mask = calculateHierarchyMask(primaryStencil, 1, 1, 1);
            if(mask < 256) {
                return ValidationResult.OK;
            }
            return ValidationResult.STENCIL_HIERARCHY_EXCEEDED;
        }

    }

}



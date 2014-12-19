package com.dynamo.cr.guied.util;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

import javax.media.opengl.GL;
import javax.media.opengl.GL2;

import com.dynamo.cr.sceneed.core.Node;

import com.dynamo.cr.guied.core.ClippingNode;
import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.ClippingNode.ClippingState;

public class Clipping {
    @SuppressWarnings("serial")
    private static class BitOverflowException extends Exception {
        ClippingNode offender;
        int overflow;

        public BitOverflowException(ClippingNode offender, int overflow) {
            this.offender = offender;
            this.overflow = overflow;
        }
    }

    public static boolean showClippingNodesEnabled = true;

    public static float[] OUTLINE_COLOR = new float[] {1.0f, 0.6f, 0.0f, 1.0f};
    public static float[] OUTLINE_SELECTED_COLOR = new float[] {1.0f, 1.0f, 0.5f, 1.0f};

    public static void showClippingNodes(boolean show) {
        showClippingNodesEnabled = show;
    }

    public static boolean getShowClippingNodes() {
        return showClippingNodesEnabled;
    }

    private static int calcBitRange(int val) {
        int bitRange = 0;
        while (val != 0) {
            bitRange++;
            val >>= 1;
        }
        return bitRange;
    }

    private static int calcMask(int bits) {
        return (1 << bits) - 1;
    }

    private static void updateStencilStates(ClippingNode clipper, int index, int nonInvClipperCount, int invClipperCount, int bitFieldOffset, ClippingState parentState) throws BitOverflowException {
        if (clipper.isStencil()) {
            ClippingState state = new ClippingState();
            ClippingState childState = new ClippingState();
            int bitRange = calcBitRange(nonInvClipperCount);
            // state used for drawing the clipper
            state.stencilWriteMask = 0xff;
            state.stencilMask = 0;
            if (parentState != null) {
                state.stencilMask = parentState.stencilMask;
            }
            boolean inverted = clipper.getClippingInverted();
            if (!inverted) {
                state.stencilRefVal = (index + 1) << bitFieldOffset;
                if (parentState != null) {
                    state.stencilRefVal |= parentState.stencilRefVal; 
                }
            } else {
                state.stencilRefVal = 1 << (7 - index);
                if (parentState != null) {
                    state.stencilRefVal |= (calcMask(bitFieldOffset) & parentState.stencilRefVal);
                }
            }
            Arrays.fill(state.colorMask, false);
            // state used for drawing any sub non-clippers
            childState.stencilWriteMask = 0;
            if (!inverted) {
                childState.stencilRefVal = state.stencilRefVal;
                childState.stencilMask = (calcMask(bitRange) << bitFieldOffset) | state.stencilMask;
            } else {
                childState.stencilRefVal = 0;
                childState.stencilMask = state.stencilRefVal;
                if (parentState != null) {
                    childState.stencilRefVal |= parentState.stencilRefVal;
                    childState.stencilMask |= parentState.stencilMask;
                }
            }
            Arrays.fill(childState.colorMask, true);
            clipper.setClippingState(state);
            clipper.setChildClippingState(childState);
            // Check for overflow
            int invertedCount = 0;
            if (inverted) {
                invertedCount = index + 1;
            } else {
                invertedCount = invClipperCount;
            }
            int bitCount = invertedCount + bitFieldOffset + bitRange;
            if (bitCount > 8) {
                throw new BitOverflowException(clipper, bitCount - 8);
            }
        }
    }

    private static int updateInvStencilStates(List<Node> nodes, int offset, int bitFieldOffset, ClippingNode parentClipper, List<ClippingNode> outNonInvClippers) throws BitOverflowException {
        int newOffset = offset;
        for (Node node : nodes) {
            if (node instanceof ClippingNode) {
                ClippingNode clipper = (ClippingNode)node;
                if (clipper.isStencil()) {
                    if (clipper.getClippingInverted()) {
                        ClippingState parentState = null;
                        if (parentClipper != null) {
                            parentState = parentClipper.getChildClippingState();
                        }
                        updateStencilStates(clipper, newOffset++, 0, 0, bitFieldOffset, parentState);
                        newOffset += updateInvStencilStates(node.getChildren(), newOffset, bitFieldOffset, clipper, outNonInvClippers);
                    } else {
                        outNonInvClippers.add(clipper);
                    }
                } else {
                    clipper.setClippingState(null);
                    clipper.setChildClippingState(null);
                    newOffset += updateInvStencilStates(node.getChildren(), newOffset, bitFieldOffset, parentClipper, outNonInvClippers);
                }
            } else {
                newOffset += updateInvStencilStates(node.getChildren(), newOffset, bitFieldOffset, parentClipper, outNonInvClippers);
            }
        }
        return newOffset - offset;
    }

    private static void updateChildren(List<Node> nodes, int bitFieldOffset, int invStencilCount, ClippingNode parentClipper) throws BitOverflowException {
        List<ClippingNode> nonInvClippers = new ArrayList<ClippingNode>();
        int invClipperCount = updateInvStencilStates(nodes, invStencilCount, bitFieldOffset, parentClipper, nonInvClippers);
        int nonInvClipperCount = nonInvClippers.size();
        int nonInvIndex = 0;
        for (ClippingNode clipper : nonInvClippers) {
            ClippingState parentState = null;
            ClippingNode p = clipper.getClosestParentClippingNode();
            if (p != null) {
                parentState = p.getChildClippingState();
            }
            updateStencilStates(clipper, nonInvIndex, nonInvClipperCount, invClipperCount + invStencilCount, bitFieldOffset, parentState);
            updateChildren(clipper.getChildren(), calcBitRange(nonInvClipperCount) + bitFieldOffset, invClipperCount, clipper);
            ++nonInvIndex;
        }
    }

    private static void collectRootClippers(Node parent, List<Node> outClippers) {
        for (Node n : parent.getChildren()) {
            if (n instanceof ClippingNode) {
                ClippingNode clipper = (ClippingNode)n;
                if (clipper.isStencil()) {
                    outClippers.add(clipper);
                    continue;
                }
                // reset non-clipper
                clipper.setClippingState(null);
                clipper.setChildClippingState(null);
            }
            collectRootClippers(n, outClippers);
        }
    }

    private static ClippingNode getRootClipper(GuiNode n) {
        ClippingNode parent = n.getClosestParentClippingNode();
        if (parent == null) {
            if (n instanceof ClippingNode) {
                parent = (ClippingNode)n;
            }
        }
        ClippingNode last = parent;
        while (parent != null) {
            last = parent;
            parent = parent.getClosestParentClippingNode();
        }
        return last;
    }

    public static void updateClippingStates(GuiSceneNode node) {
        List<Node> rootClippers = new ArrayList<Node>();
        List<ClippingNode> clearClippers = new ArrayList<ClippingNode>();
        collectRootClippers(node.getNodesNode(), rootClippers);
        List<List<Node>> rootIntervals = new ArrayList<List<Node>>();
        rootIntervals.add(rootClippers);
        while (!rootIntervals.isEmpty()) {
            List<Node> rootInterval = rootIntervals.remove(0);
            if (!rootInterval.isEmpty()) {
                try {
                    updateChildren(rootInterval, 0, 0, null);
                } catch (BitOverflowException e) {
                    ClippingNode root = getRootClipper(e.offender);
                    int rootCount = rootInterval.size();
                    int rootBitRange = calcBitRange(rootCount);
                    boolean recoverable = root != null && rootBitRange - 1 >= e.overflow;
                    if (recoverable) {
                        clearClippers.add(root);
                        rootIntervals.add(Collections.singletonList((Node)root));
                        List<Node> remainders = rootClippers.subList(rootClippers.indexOf(root) + 1, rootClippers.size());
                        if (!remainders.isEmpty()) {
                            clearClippers.add((ClippingNode)remainders.get(0));
                            rootIntervals.add(remainders);
                        }
                    } else {
                        e.offender.getClippingState().overflow = true;
                        return;
                    }
                }
            }
        }
        for (ClippingNode clipper : clearClippers) {
            clipper.getClippingState().stencilClearBuffer = true;
        }
    }

    public static void beginClipping(GL2 gl) {
        gl.glEnable(GL.GL_STENCIL_TEST);
    }

    public static void endClipping(GL2 gl) {
        gl.glDisable(GL.GL_STENCIL_TEST);
        gl.glColorMask(true, true, true, true);
    }

    public static void setupClipping(GL2 gl, ClippingState state) {
        gl.glStencilOp(GL.GL_KEEP, GL.GL_REPLACE, GL.GL_REPLACE);
        gl.glStencilFunc(GL2.GL_EQUAL, state.stencilRefVal, state.stencilMask);
        gl.glStencilMask(state.stencilWriteMask);
        if (state.stencilClearBuffer) {
            gl.glClear(GL.GL_STENCIL_BUFFER_BIT);
        }
        boolean[] c = state.colorMask;
        gl.glColorMask(c[0], c[1], c[2], c[3]);
    }
}



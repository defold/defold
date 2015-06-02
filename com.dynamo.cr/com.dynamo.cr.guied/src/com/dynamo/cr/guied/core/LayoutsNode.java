package com.dynamo.cr.guied.core;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.sceneed.core.Node;

import java.util.ArrayList;
import java.util.List;

@SuppressWarnings("serial")
public class LayoutsNode extends LabelNode {

    public LayoutsNode() {
        super("Layouts");
    }

    public List<String> getLayouts() {
        List<Node> layoutNodes = this.getChildren();
        List<String> layouts = new ArrayList<String>(layoutNodes.size());
        for (Node node : layoutNodes) {
            LayoutNode layoutNode = (LayoutNode) node;
            layouts.add(layoutNode.getId());
        }
        return layouts;
    }

    public Node getLayoutNode(String id) {
        List<Node> layoutNodes = this.getChildren();
        for (Node node : layoutNodes) {
            LayoutNode layoutNode = (LayoutNode) node;
            if(layoutNode.getId().compareTo(id)==0) {
                return layoutNode;
            }
        }
        return null;
    }

    @Override
    protected void childRemoved(Node child)
    {
        String deletedLayoutId = ((LayoutNode)child).getId();
        super.childRemoved(child);
        GuiSceneNode scene = (GuiSceneNode) this.getModel().getRoot();
        scene.setDefaultLayout();
        GuiNodeStateBuilder.removeStates(scene.getNodesNode().getChildren(), deletedLayoutId);
    };

}

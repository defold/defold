package com.dynamo.cr.guied.core.propertytesters;

import org.eclipse.core.expressions.PropertyTester;
import org.eclipse.jface.viewers.IStructuredSelection;

import com.dynamo.cr.guied.core.GuiNode;
import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.core.LayoutNode;

public class GuiSelectionPropertyTester extends PropertyTester {

    public GuiSelectionPropertyTester() {
    }

    @Override
    public boolean test(Object receiver, String property, Object[] args, Object expectedValue) {
        if (receiver instanceof IStructuredSelection) {
            IStructuredSelection selection = (IStructuredSelection)receiver;
            if (property.equals("defaultlayout")) {
                Object[] objects = selection.toArray();
                for (Object object : objects) {
                    if (object instanceof LayoutNode) {
                        LayoutNode node = (LayoutNode) object;
                        GuiSceneNode scene = (GuiSceneNode) node.getParent().getParent();
                        if(scene.getDefaultLayout().getId().compareTo(node.getId())==0) {
                            return true;
                        }
                    }
                }
            } else if (property.equals("isTemplateNodeChild")) {
                Object[] objects = selection.toArray();
                for (Object object : objects) {
                    if (object instanceof GuiNode) {
                        GuiNode node = (GuiNode) object;
                        if(node.isTemplateNodeChild()) {
                            return true;
                        }
                    }
                }
            }
        }
        return false;
    }

}

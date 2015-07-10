package com.dynamo.cr.guied.core;

import java.io.IOException;
import java.util.Comparator;
import java.util.List;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.guied.util.GuiNodeStateBuilder;
import com.dynamo.cr.guied.util.GuiTemplateBuilder;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;


@SuppressWarnings("serial")
public class TemplateNode extends GuiNode {

    private GuiSceneNode templateScene = null;

    @Property(editorType = EditorType.RESOURCE, extensions = "gui")
    @Resource
    @NotEmpty
    private String template = "";

    public boolean isSizeVisible() {
        return false;
    }

    public boolean isColorVisible() {
        return false;
    }

    public boolean isBlendModeVisible() {
        return false;
    }

    public boolean isPivotVisible() {
        return false;
    }

    public boolean isAdjustModeVisible() {
        return false;
    }

    public boolean isXanchorVisible() {
        return false;
    }

    public boolean isYanchorVisible() {
        return false;
    }

    @Override
    public void setId(String id) {
        if(!isTemplateNodeChild() && !this.getId().equals(id)) {
            refactorChildHierarchyIds(this.getChildren(), id, getId().length()+1);
        }
        super.setId(id);
    }

    public void setTemplatePath(String templatePath) {
        this.template = templatePath;
    }

    public String getTemplatePath() {
        return this.template;
    }

    public String getTemplate() {
        return getTemplatePath();
    }

    public void setTemplate(String templatePath) throws IOException, CoreException {
        if(getTemplatePath().equals(templatePath)) {
            return;
        }
        setTemplatePath(templatePath);
        GuiTemplateBuilder.build(this);
    }

    public boolean isTemplateEditable() {
        return !isTemplateNodeChild();
    }

    public void setTemplateScene(GuiSceneNode scene) {
        this.templateScene = scene;
    }

    public GuiSceneNode getTemplateScene() {
        return this.templateScene;
    }

    @Override
    public boolean handleReload(IFile file, boolean childWasReloaded) {
        IFile templateFile = getModel().getFile(getTemplatePath());
        if ((templateFile.exists() && templateFile.equals(file)) || childWasReloaded) {
            if(!this.isTemplateNodeChild()) {
                String currentLayout = getScene().getCurrentLayout().getId();
                getScene().setDefaultLayout();
                getModel().setSelection(new StructuredSelection(this));
                boolean result = GuiTemplateBuilder.build(this);
                getScene().setCurrentLayout(currentLayout);
                return result;
            }
            return true;
        }
        return false;
    }

    public void refactorChildHierarchyIds(List<Node> nodes, String newId, int offset) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            refactorChildHierarchyIds(node.getChildren(), newId, offset);
            guiNode.setId(newId + "/" + guiNode.getId().substring(offset));
        }
    }

    public void setParentTemplateRecursive(List<Node> nodes) {
        for(Node node : nodes) {
            GuiNode guiNode = (GuiNode) node;
            guiNode.setParentTemplateNode(this);
            setParentTemplateRecursive(node.getChildren());
        }
    }

    public void sortChildren() {
        sortChildren(new Comparator<Node>() {
            @Override
            public int compare(Node o1, Node o2) {
                if (o1 instanceof GuiNode && o2 instanceof GuiNode) {
                    String id1 = ((GuiNode)o1).getId();
                    String id2 = ((GuiNode)o2).getId();
                    return id1.compareTo(id2);
                } else if (o1 instanceof ComponentNode && o2 instanceof ComponentNode) {
                    String id1 = ((ComponentNode)o1).getId();
                    String id2 = ((ComponentNode)o2).getId();
                    return id1.compareTo(id2);
                } else {
                    if (o1 instanceof ComponentNode) {
                        return 1;
                    } else {
                        return -1;
                    }
                }
            }
        });
    }

    @Override
    public void setModel(ISceneModel model) {
        if(model != this.getModel()) {
            super.setModel(model);
            Node parent = getParent();
            while(parent != null)
            {
                if(parent instanceof TemplateNode) {
                    break;
                }
                parent = parent.getParent();
            }
            if(parent == null) {
                if (model != null) {
                    getModel().setSelection(new StructuredSelection(this));
                    GuiTemplateBuilder.build(this);
                }
            }
        }
    }

    @Override
    public Image getIcon() {
        if(GuiNodeStateBuilder.isStateSet(this)) {
            if(isTemplateNodeChild()) {
                return Activator.getDefault().getImageRegistry().get(Activator.TEMPLATE_NODE_OVERRIDDEN_TEMPLATE_IMAGE_ID);
            }
            return Activator.getDefault().getImageRegistry().get(Activator.TEMPLATE_NODE_OVERRIDDEN_IMAGE_ID);
        }
        return Activator.getDefault().getImageRegistry().get(Activator.TEMPLATE_NODE_IMAGE_ID);
    }

}

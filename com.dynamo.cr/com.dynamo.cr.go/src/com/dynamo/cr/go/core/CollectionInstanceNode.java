package com.dynamo.cr.go.core;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;
import org.eclipse.swt.graphics.Image;

import com.dynamo.cr.go.Constants;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.sceneed.core.ISceneModel;

@SuppressWarnings("serial")
public class CollectionInstanceNode extends InstanceNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"collection"})
    @Resource
    @NotEmpty
    private String collection = "";

    private CollectionNode collectionNode;

    public CollectionInstanceNode(CollectionNode collection) {
        super();
        this.collectionNode = collection;
        if (this.collectionNode != null) {
            this.collectionNode.setFlagsRecursively(Flags.LOCKED);
            addChild(this.collectionNode);
        }
    }

    public String getCollection() {
        return this.collection;
    }

    public void setCollection(String collection) {
        this.collection = collection;
        reloadCollection();
    }

    public IStatus validateCollection() {
        if (getModel() != null && !this.collection.isEmpty()) {
            if (this.collectionNode == null) {
                int index = this.collection.lastIndexOf('.');
                String message = null;
                if (index < 0) {
                    message = NLS.bind(Messages.CollectionInstanceNode_collection_UNKNOWN_TYPE, this.collection);
                } else {
                    String ext = this.collection.substring(index+1);
                    message = NLS.bind(Messages.CollectionInstanceNode_collection_INVALID_TYPE, ext);
                }
                return new Status(IStatus.ERROR, Constants.PLUGIN_ID, message);
            }
        }
        return Status.OK_STATUS;
    }

    @Override
    public String toString() {
        return String.format("%s (%s)", getId(), this.collection);
    }

    @Override
    public boolean handleReload(IFile file) {
        IFile collectionFile = getModel().getFile(this.collection);
        if (collectionFile.exists() && collectionFile.equals(file)) {
            if (reloadCollection()) {
                return true;
            }
        }
        return false;
    }

    private boolean reloadCollection() {
        ISceneModel model = getModel();
        if (model != null) {
            try {
                clearChildren();
                this.collectionNode = (CollectionNode)model.loadNode(this.collection);
                if (this.collectionNode != null) {
                    this.collectionNode.setModel(model);
                    this.collectionNode.setPath(this.collection);
                    this.collectionNode.setFlagsRecursively(Flags.LOCKED);
                    addChild(this.collectionNode);
                }
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateComponent below
            }
            return true;
        } else if (this.collectionNode != null) {
            this.collectionNode.setPath(this.collection);
        }
        return false;
    }

    @Override
    public Image getIcon() {
        if (this.collectionNode != null) {
            return this.collectionNode.getIcon();
        }
        return super.getIcon();
    }

}

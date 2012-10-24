package com.dynamo.cr.ddfeditor.scene;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.osgi.util.NLS;

import com.dynamo.cr.ddfeditor.Activator;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.ISceneModel;

@SuppressWarnings("serial")
public class CollectionProxyNode extends ComponentTypeNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"collection"})
    @Resource
    @NotEmpty
    private String collection = "";

    private CollectionNode collectionNode;

    public CollectionProxyNode() {
        super();
    }

    public String getCollection() {
        return this.collection;
    }

    public void setCollection(String collection) {
        this.collection = collection;
        reloadCollection();
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        if (model != null && this.collectionNode == null && !this.collection.isEmpty()) {
            reloadCollection();
        }
    }

    public IStatus validateCollection() {
        if (getModel() != null && !this.collection.isEmpty()) {
            IStatus collectionStatus = this.collectionNode.getStatus();
            if (collectionStatus != null) {
                if (!collectionStatus.isOK()) {
                    return new Status(IStatus.ERROR, Activator.PLUGIN_ID, Messages.CollectionProxyNode_collection_INVALID_REFERENCE);
                }
            } else {
                int index = this.collection.lastIndexOf('.');
                String message = null;
                if (index < 0) {
                    message = NLS.bind(Messages.CollectionProxyNode_collection_UNKNOWN_TYPE, this.collection);
                } else {
                    String ext = this.collection.substring(index+1);
                    message = NLS.bind(Messages.CollectionProxyNode_collection_INVALID_TYPE, ext);
                }
                return new Status(IStatus.ERROR, Activator.PLUGIN_ID, message);
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
        if (!this.collection.isEmpty()) {
            IFile collectionFile = getModel().getFile(this.collection);
            if (collectionFile.exists() && collectionFile.equals(file)) {
                if (reloadCollection()) {
                    return true;
                }
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
                    this.collectionNode.setFlagsRecursively(Flags.LOCKED);
                    addChild(this.collectionNode);
                }
            } catch (Throwable e) {
                // no reason to handle exception since having a null type is invalid state, will be caught in validateCollection above
            }
            return true;
        }
        return false;
    }
}

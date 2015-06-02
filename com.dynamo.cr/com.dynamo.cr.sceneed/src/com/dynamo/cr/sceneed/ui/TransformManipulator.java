package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.List;

import javax.vecmath.Matrix4d;
import javax.vecmath.Vector3d;

import org.eclipse.swt.events.MouseEvent;

import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Manipulator;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation;
import com.dynamo.cr.sceneed.core.operations.TransformNodeOperation.ITransformer;

@SuppressWarnings("serial")
public abstract class TransformManipulator<T> extends RootManipulator {

    private boolean transformChanged = false;
    private List<T> originalLocalTransforms = new ArrayList<T>();
    private List<T> newLocalTransforms = new ArrayList<T>();

    protected abstract String getOperation();

    protected abstract ITransformer<T> getTransformer();

    protected abstract void applyTransform(List<Node> selection, List<T> originalLocalTransforms);

    @Override
    protected final void transformChanged() {
        this.transformChanged = true;
        applyTransform(getSelection(), this.originalLocalTransforms);
    }

    @Override
    protected void selectionChanged() {
        List<Node> sel = getSelection();
        this.originalLocalTransforms.clear();
        ITransformer<T> transformer = getTransformer();
        for (Node node : sel) {
            T transform = transformer.get(node);
            this.originalLocalTransforms.add(transform);
        }
    }

    @Override
    public void manipulatorChanged(Manipulator manipulator) {
    }

    @Override
    public void refresh() {
        List<Node> sel = getSelection();
        if (!sel.isEmpty()) {
            Node n = sel.get(0);
            Matrix4d world = new Matrix4d();
            n.getWorldTransform(world);
            Vector3d translation = new Vector3d();
            world.get(translation);
            this.translation.set(translation);
        }
    }

    @Override
    public boolean match(Object[] selection) {
        for (Object object : selection) {
            if (!(object instanceof Node) || !((Node) object).isFlagSet(Node.Flags.TRANSFORMABLE)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public void mouseUp(MouseEvent e) {
        if (transformChanged) {
            ITransformer<T> transformer = getTransformer();
            for (Node node : getSelection()) {
                T transform = transformer.get(node);
                newLocalTransforms.add(transform);
            }

            TransformNodeOperation<T> operation = new TransformNodeOperation<T>(getOperation(), getTransformer(),
                    getSelection(), originalLocalTransforms, newLocalTransforms);
            getController().executeOperation(operation);
            transformChanged = false;
        }
    }

    @SuppressWarnings("unchecked")
    public IPropertyAccessor<Object, ISceneModel> getNodePropertyAccessor(Node n) {
        PropertyIntrospector<Node, ISceneModel> introspectors = (PropertyIntrospector<Node, ISceneModel>) n.getAdapter(PropertyIntrospector.class);
        return (IPropertyAccessor<Object, ISceneModel>) (IPropertyAccessor<?, ? extends IPropertyObjectWorld>) introspectors.getAccessor();
    }

}

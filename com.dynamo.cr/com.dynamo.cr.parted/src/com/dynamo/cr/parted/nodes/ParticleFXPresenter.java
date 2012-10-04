package com.dynamo.cr.parted.nodes;

import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.swt.graphics.Image;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.parted.ParticleEditorPlugin;
import com.dynamo.cr.parted.operations.AddEmitterOperation;
import com.dynamo.cr.parted.operations.AddModifierOperation;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;

public class ParticleFXPresenter implements ISceneView.INodePresenter<ParticleFXNode> {


    private static Logger logger = LoggerFactory.getLogger(ParticleFXPresenter.class);
    public ParticleFXPresenter() {
    }

    private ParticleFXNode findRoot(IPresenterContext presenterContext) {
        IStructuredSelection selection = presenterContext.getSelection();
        if (selection.isEmpty())
            return null;

        Node parent = (Node) selection.getFirstElement();
        while (parent.getParent() != null) {
            parent = parent.getParent();
        }
        return (ParticleFXNode) parent;
    }

    private EmitterNode findEmitter(IPresenterContext presenterContext) {
        IStructuredSelection selection = presenterContext.getSelection();
        if (selection.isEmpty())
            return null;

        Node parent = (Node) selection.getFirstElement();
        while (parent != null && !(parent instanceof EmitterNode)) {
            parent = parent.getParent();
        }
        return (EmitterNode) parent;
    }

    public void onAddEmitter(IPresenterContext presenterContext, ILoaderContext loaderContext) {
        ParticleFXNode parent = findRoot(presenterContext);
        if (parent == null)
            return;

        try {
            EmitterNode emitter = (EmitterNode) loaderContext.loadNodeFromTemplate("emitter");
            presenterContext.executeOperation(new AddEmitterOperation(parent, emitter, presenterContext));
        } catch (Exception e) {
            logger.error("Failed to load emitter template", e);
        }
    }

    public void onAddModifier(IPresenterContext presenterContext,
            ILoaderContext loaderContext) {

        EmitterNode parent = findEmitter(presenterContext);
        if (parent == null)
            return;

        LabelProvider labelProvider = new LabelProvider() {

            @Override
            public Image getImage(Object element) {
                return ParticleEditorPlugin.getDefault().getIconByExtension(((INodeType)element).getResourceType().getFileExtension());
            }

            @Override
            public String getText(Object element) {
                return ((INodeType) element).getResourceType().getName();
            }
        };

        INodeType[] emitterTypes = new INodeType[] { presenterContext.getNodeType(AccelerationNode.class),
                                                     presenterContext.getNodeType(DragNode.class) };
        INodeType emitterType = (INodeType) presenterContext.selectFromArray("Add Emitter", "Select a emitter type:", emitterTypes, labelProvider);
        if (emitterType != null) {
            try {
                ModifierNode child = (ModifierNode) loaderContext.loadNodeFromTemplate(emitterType.getExtension());
                presenterContext.executeOperation(new AddModifierOperation(parent, child, presenterContext));
            } catch (Exception e) {
                throw new RuntimeException("Failed to add modifier", e);
            }
        }
    }

}

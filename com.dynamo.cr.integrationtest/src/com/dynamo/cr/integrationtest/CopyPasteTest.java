package com.dynamo.cr.integrationtest;

import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.CollectionProxyNode;
import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.ConvexShapeNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.luaeditor.scene.ScriptNode;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.physics.proto.Physics.ConvexShape;

public class CopyPasteTest extends AbstractSceneTest {

    @Test
    public void testGameObjectInstance() throws Exception {
        CollectionNode collection = new CollectionNode();
        GameObjectInstanceNode instance = new GameObjectInstanceNode(new GameObjectNode());

        testCopyPaste(collection, instance);
    }

    @Test
    public void testConvexShape() throws Exception {
        CollisionObjectNode co = new CollisionObjectNode();
        ConvexShape.Builder builder = ConvexShape.newBuilder();
        ConvexShapeNode shape = new ConvexShapeNode(builder.setShapeType(ConvexShape.Type.TYPE_SPHERE).addData(1.0f).build());

        testCopyPaste(co, shape);
    }

    @Test
    public void testScript() throws Exception {
        GameObjectNode go = new GameObjectNode();
        ScriptNode script = new ScriptNode();

        testCopyPaste(go, script);
    }

    @Test
    public void testCollectionProxy() throws Exception {
        GameObjectNode go = new GameObjectNode();
        CollectionProxyNode proxy = new CollectionProxyNode();
        proxy.setCollection("/logic/main.collection");

        testCopyPaste(go, proxy);
    }

    private void testCopyPaste(Node parent, Node child) throws Exception {
        parent.addChild(child);

        select(child);

        ISceneView.IPresenter presenter = getPresenter();
        presenter.onCopySelection(getPresenterContext(), getLoaderContext(), null);

        presenter.onPasteIntoSelection(getPresenterContext());

        verifyExcecution();
    }
}

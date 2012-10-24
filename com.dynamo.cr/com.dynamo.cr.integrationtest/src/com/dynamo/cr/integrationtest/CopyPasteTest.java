package com.dynamo.cr.integrationtest;

import java.util.List;

import javax.vecmath.Quat4d;
import javax.vecmath.Vector4d;

import org.junit.Test;

import com.dynamo.cr.ddfeditor.operations.AddShapeNodeOperation;
import com.dynamo.cr.ddfeditor.scene.CollectionProxyNode;
import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.CollisionShapeNode;
import com.dynamo.cr.ddfeditor.scene.ModelNode;
import com.dynamo.cr.ddfeditor.scene.SphereCollisionShapeNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.ComponentNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.go.core.RefComponentNode;
import com.dynamo.cr.go.core.operations.AddComponentOperation;
import com.dynamo.cr.go.core.operations.AddInstanceOperation;
import com.dynamo.cr.luaeditor.scene.ScriptNode;
import com.dynamo.cr.parted.nodes.EmitterNode;
import com.dynamo.cr.parted.nodes.ParticleFXNode;
import com.dynamo.cr.parted.operations.AddEmitterOperation;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.operations.AddChildrenOperation;
import com.dynamo.cr.tileeditor.operations.AddAnimationNodeOperation;
import com.dynamo.cr.tileeditor.operations.AddCollisionGroupNodeOperation;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Emitter.ParticleProperty;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.ParticleKey;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.SplinePoint;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;

public class CopyPasteTest extends AbstractSceneTest {

    @Test
    public void testGameObjectInstance() throws Exception {
        CollectionNode collection = new CollectionNode();
        GameObjectInstanceNode instance = new GameObjectInstanceNode();
        instance.setGameObject("/game_object/test.go");

        testCopyPaste(collection, instance, new AddInstanceOperation(collection, instance, getPresenterContext()));
    }

    @Test
    public void testCollisionShape() throws Exception {
        CollisionObjectNode co = new CollisionObjectNode();
        CollisionShapeNode cs = new SphereCollisionShapeNode(new Vector4d(), new Quat4d(), new float[] {0.0f}, 0);
        testCopyPaste(co, cs, new AddShapeNodeOperation(co, cs, getPresenterContext()));
    }

    @Test
    public void testScript() throws Exception {
        GameObjectNode go = new GameObjectNode();
        ScriptNode script = new ScriptNode();
        RefComponentNode component = new RefComponentNode(script);
        component.setComponent("/script/props.script");
        testCopyPaste(go, component);
    }

    @Test
    public void testCollectionProxy() throws Exception {
        GameObjectNode go = new GameObjectNode();
        CollectionProxyNode proxy = new CollectionProxyNode();
        proxy.setCollection("/collection/test.collection");

        testCopyPaste(go, proxy);
    }

    @Test
    public void testAnimation() throws Exception {
        TileSetNode tileSet = new TileSetNode();
        AnimationNode animation = new AnimationNode();

        testCopyPaste(tileSet, animation, new AddAnimationNodeOperation(tileSet, animation, getPresenterContext()));
    }

    @Test
    public void testCollisionGroup() throws Exception {
        TileSetNode tileSet = new TileSetNode();
        CollisionGroupNode collisionGroup = new CollisionGroupNode();

        testCopyPaste(tileSet, collisionGroup, new AddCollisionGroupNodeOperation(tileSet, collisionGroup, getPresenterContext()));
    }

    @Test
    public void testParticle() throws Exception {
        Emitter.Builder eb = Emitter.newBuilder()
                .setMode(PlayMode.PLAY_MODE_LOOP)
                .setSpace(EmissionSpace.EMISSION_SPACE_EMITTER)
                .setPosition(Point3.newBuilder())
                .setRotation(Quat.newBuilder())
                .setTileSource("")
                .setAnimation("")
                .setMaterial("")
                .setMaxParticleCount(100)
                .setType(EmitterType.EMITTER_TYPE_SPHERE)
                .addParticleProperties(ParticleProperty
                        .newBuilder()
                        .setKey(ParticleKey.PARTICLE_KEY_ALPHA)
                        .addPoints(SplinePoint.newBuilder().setX(0).setY(0).setTX(0).setTY(0))
                        .addPoints(SplinePoint.newBuilder().setX(0).setY(0).setTX(0).setTY(0)));

        float duration = 123;
        EmitterNode emitter = new EmitterNode(eb.setDuration(duration).build());
        ParticleFXNode node = new ParticleFXNode();
        testCopyPaste(node, emitter, new AddEmitterOperation(node, emitter, getPresenterContext()));
    }

    @Test
    public void testModel() throws Exception {
        GameObjectNode go = new GameObjectNode();
        ModelDesc.Builder b = ModelDesc.newBuilder();
        b.setMaterial("/material/test.material");
        b.setMesh("/mesh/test.dae");
        ModelNode model = new ModelNode(b.build());

        testCopyPaste(go, model);
    }

    private void testCopyPaste(GameObjectNode gameObject, ComponentNode component) throws Exception {
        gameObject.setModel(getModel());
        doTestCopyPaste(gameObject, component, new AddComponentOperation(gameObject, component, getPresenterContext()));
    }

    private void testCopyPaste(Node parent, Node child, AddChildrenOperation operation) throws Exception {
        parent.setModel(getModel());
        doTestCopyPaste(parent, child, operation);
    }

    private void doTestCopyPaste(Node parent, Node child, AddChildrenOperation operation) throws Exception {
        getPresenterContext().executeOperation(operation);
        verifyExcecution();

        select(child);

        ISceneView.IPresenter presenter = getPresenter();
        presenter.onCopySelection(getPresenterContext(), getLoaderContext(), null);

        presenter.onPasteIntoSelection(getPresenterContext());

        verifyExcecution();
    }
}

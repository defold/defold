package com.dynamo.cr.integrationtest;

import org.junit.Test;

import com.dynamo.cr.ddfeditor.scene.CollectionProxyNode;
import com.dynamo.cr.ddfeditor.scene.CollisionObjectNode;
import com.dynamo.cr.ddfeditor.scene.ConvexShapeNode;
import com.dynamo.cr.go.core.CollectionNode;
import com.dynamo.cr.go.core.GameObjectInstanceNode;
import com.dynamo.cr.go.core.GameObjectNode;
import com.dynamo.cr.luaeditor.scene.ScriptNode;
import com.dynamo.cr.parted.EmitterNode;
import com.dynamo.cr.parted.ParticleFXNode;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.tileeditor.scene.AnimationNode;
import com.dynamo.cr.tileeditor.scene.CollisionGroupNode;
import com.dynamo.cr.tileeditor.scene.TileSetNode;
import com.dynamo.particle.proto.Particle.EmissionSpace;
import com.dynamo.particle.proto.Particle.Emitter;
import com.dynamo.particle.proto.Particle.Emitter.ParticleProperty;
import com.dynamo.particle.proto.Particle.EmitterType;
import com.dynamo.particle.proto.Particle.ParticleKey;
import com.dynamo.particle.proto.Particle.PlayMode;
import com.dynamo.particle.proto.Particle.SplinePoint;
import com.dynamo.physics.proto.Physics.ConvexShape;
import com.dynamo.proto.DdfMath.Point3;
import com.dynamo.proto.DdfMath.Quat;

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

    @Test
    public void testAnimation() throws Exception {
        TileSetNode tileSet = new TileSetNode();
        AnimationNode animation = new AnimationNode();

        testCopyPaste(tileSet, animation);
    }

    @Test
    public void testCollisionGroup() throws Exception {
        TileSetNode tileSet = new TileSetNode();
        CollisionGroupNode collisionGroup = new CollisionGroupNode();

        testCopyPaste(tileSet, collisionGroup);
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
        node.addChild(emitter);
        testCopyPaste(node, emitter);
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

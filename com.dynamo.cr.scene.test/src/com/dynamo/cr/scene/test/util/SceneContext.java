package com.dynamo.cr.scene.test.util;

import com.dynamo.cr.scene.graph.AbstractNodeLoaderFactory;
import com.dynamo.cr.scene.graph.CameraNodeLoader;
import com.dynamo.cr.scene.graph.CollectionNodeLoader;
import com.dynamo.cr.scene.graph.CollisionNodeLoader;
import com.dynamo.cr.scene.graph.LightNodeLoader;
import com.dynamo.cr.scene.graph.MeshNodeLoader;
import com.dynamo.cr.scene.graph.ModelNodeLoader;
import com.dynamo.cr.scene.graph.PrototypeNodeLoader;
import com.dynamo.cr.scene.graph.Scene;
import com.dynamo.cr.scene.graph.SpriteNodeLoader;
import com.dynamo.cr.scene.resource.CameraLoader;
import com.dynamo.cr.scene.resource.CollisionLoader;
import com.dynamo.cr.scene.resource.ConvexShapeLoader;
import com.dynamo.cr.scene.resource.LightLoader;
import com.dynamo.cr.scene.resource.SpriteLoader;
import com.dynamo.cr.scene.resource.TextureLoader;
import com.dynamo.cr.scene.test.graph.FileNodeLoaderFactory;
import com.dynamo.cr.scene.test.graph.FileResourceLoaderFactory;

public class SceneContext {
    public AbstractNodeLoaderFactory factory;
    public FileResourceLoaderFactory resourceFactory;
    public Scene scene;

    public SceneContext() {
        String root = "test";
        resourceFactory = new FileResourceLoaderFactory(root);
        resourceFactory.addLoader(new TextureLoader(), "png");
        resourceFactory.addLoader(new CameraLoader(), "camera");
        resourceFactory.addLoader(new LightLoader(), "light");
        resourceFactory.addLoader(new SpriteLoader(), "sprite");
        resourceFactory.addLoader(new CollisionLoader(), "collisionobject");
        resourceFactory.addLoader(new ConvexShapeLoader(), "convexshape");
        factory = new FileNodeLoaderFactory(root, resourceFactory);
        factory.addLoader(new CollectionNodeLoader(), "collection");
        factory.addLoader(new PrototypeNodeLoader(), "go");
        factory.addLoader(new ModelNodeLoader(), "model");
        factory.addLoader(new MeshNodeLoader(), "dae");
        factory.addLoader(new CameraNodeLoader(), "camera");
        factory.addLoader(new LightNodeLoader(), "light");
        factory.addLoader(new SpriteNodeLoader(), "sprite");
        factory.addLoader(new CollisionNodeLoader(), "collisionobject");
        scene = new Scene();
    }
}

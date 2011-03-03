package com.dynamo.cr.contenteditor.test;

import com.dynamo.cr.contenteditor.resource.CameraLoader;
import com.dynamo.cr.contenteditor.resource.CollisionLoader;
import com.dynamo.cr.contenteditor.resource.ConvexShapeLoader;
import com.dynamo.cr.contenteditor.resource.LightLoader;
import com.dynamo.cr.contenteditor.resource.SpriteLoader;
import com.dynamo.cr.contenteditor.resource.TextureLoader;
import com.dynamo.cr.contenteditor.scene.AbstractNodeLoaderFactory;
import com.dynamo.cr.contenteditor.scene.CameraNodeLoader;
import com.dynamo.cr.contenteditor.scene.CollectionNodeLoader;
import com.dynamo.cr.contenteditor.scene.CollisionNodeLoader;
import com.dynamo.cr.contenteditor.scene.LightNodeLoader;
import com.dynamo.cr.contenteditor.scene.MeshNodeLoader;
import com.dynamo.cr.contenteditor.scene.ModelNodeLoader;
import com.dynamo.cr.contenteditor.scene.PrototypeNodeLoader;
import com.dynamo.cr.contenteditor.scene.Scene;
import com.dynamo.cr.contenteditor.scene.SpriteNodeLoader;

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

package com.dynamo.cr.sceneed.gameobject;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import org.eclipse.core.runtime.CoreException;

import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.NodePresenter;
import com.dynamo.gamesystem.proto.GameSystem.CollectionProxyDesc;
import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.sprite.proto.Sprite.SpriteDesc;
import com.google.protobuf.TextFormat;

/**
 * Place holder class to support generic components, will be replaced with component-specific presenters in time.
 * @author rasv
 *
 */
public class ComponentPresenter extends NodePresenter {

    @Override
    public Node load(String type, InputStream contents) throws IOException, CoreException {
        if (type.equals("sprite")) {
            InputStreamReader reader = new InputStreamReader(contents);
            SpriteDesc.Builder builder = SpriteDesc.newBuilder();
            TextFormat.merge(reader, builder);
            SpriteDesc desc = builder.build();
            SpriteNode sprite = new SpriteNode();
            sprite.setTexture(desc.getTexture());
            sprite.setWidth(desc.getWidth());
            sprite.setHeight(desc.getHeight());
            sprite.setTileWidth(desc.getTileWidth());
            sprite.setTileHeight(desc.getTileHeight());
            sprite.setTilesPerRow(desc.getTilesPerRow());
            sprite.setTileCount(desc.getTileCount());
            return sprite;
        } else if (type.equals("spawnpoint")) {
            InputStreamReader reader = new InputStreamReader(contents);
            SpawnPointDesc.Builder builder = SpawnPointDesc.newBuilder();
            TextFormat.merge(reader, builder);
            SpawnPointDesc desc = builder.build();
            SpawnPointNode spawnPoint = new SpawnPointNode();
            spawnPoint.setPrototype(desc.getPrototype());
            return spawnPoint;
        } else if (type.equals("collisionobject")) {
            InputStreamReader reader = new InputStreamReader(contents);
            CollisionObjectDesc.Builder builder = CollisionObjectDesc.newBuilder();
            TextFormat.merge(reader, builder);
            CollisionObjectDesc desc = builder.build();
            CollisionObjectNode collisionObject = new CollisionObjectNode();
            collisionObject.setCollisionShape(desc.getCollisionShape());
            collisionObject.setType(desc.getType());
            collisionObject.setMass(desc.getMass());
            collisionObject.setFriction(desc.getFriction());
            collisionObject.setRestitution(desc.getRestitution());
            collisionObject.setGroup(desc.getGroup());
            StringBuffer mask = new StringBuffer();
            int n = desc.getMaskCount();
            for (int i = 0; i < n; ++i) {
                mask.append(desc.getMask(i));
                if (i < n - 1) {
                    mask.append(", ");
                }
            }
            collisionObject.setMask(mask.toString());
            return collisionObject;
        } else if (type.equals("collectionproxy")) {
            InputStreamReader reader = new InputStreamReader(contents);
            CollectionProxyDesc.Builder builder = CollectionProxyDesc.newBuilder();
            TextFormat.merge(reader, builder);
            CollectionProxyDesc desc = builder.build();
            CollectionProxyNode collectionProxy = new CollectionProxyNode();
            collectionProxy.setCollection(desc.getCollection());
            return collectionProxy;
        }
        return new GenericComponentTypeNode(type);
    }

    @Override
    public Node create(String type) throws IOException, CoreException {
        if (type.equals("sprite")) {
            return new SpriteNode();
        } else if (type.equals("spawnpoint")) {
            return new SpawnPointNode();
        } else if (type.equals("collisionobject")) {
            return new CollisionObjectNode();
        } else if (type.equals("collectionproxy")) {
            return new CollectionProxyNode();
        } else {
            return new GenericComponentTypeNode(type);
        }
    }

}

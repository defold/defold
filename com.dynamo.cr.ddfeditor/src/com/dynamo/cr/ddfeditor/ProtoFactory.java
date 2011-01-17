package com.dynamo.cr.ddfeditor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

import com.dynamo.camera.proto.Camera.CameraDesc;
import com.dynamo.gameobject.proto.GameObject;
import com.dynamo.gameobject.proto.GameObject.CollectionDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightDesc;
import com.dynamo.gamesystem.proto.GameSystem.LightType;
import com.dynamo.gamesystem.proto.GameSystem.SpawnPointDesc;
import com.dynamo.gui.proto.Gui;
import com.dynamo.input.proto.Input.InputBinding;
import com.dynamo.model.proto.Model.ModelDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectDesc;
import com.dynamo.physics.proto.Physics.CollisionObjectType;
import com.dynamo.proto.DdfMath.Vector3;
import com.dynamo.render.proto.Font.FontDesc;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Render.RenderPrototypeDesc;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class ProtoFactory {
    public static class ComponentType {
        public String name;
        public String extension;

        ComponentType(String name, String extension) {
            this.name = name;
            this.extension = extension;
        }

        @Override
        public String toString() {
            return this.name;
        }
    }

    static Map<String, Descriptor> extensionToDescriptor = new HashMap<String, Descriptor>();
    static Map<String, Message> extensionToTemplate = new HashMap<String, Message>();
    static ComponentType[] embeddedComponentTypes;

    static {
        extensionToDescriptor.put("model", ModelDesc.getDescriptor());
        extensionToDescriptor.put("camera", CameraDesc.getDescriptor());
        extensionToDescriptor.put("light", LightDesc.getDescriptor());
        extensionToDescriptor.put("spawnpoint", SpawnPointDesc.getDescriptor());
        extensionToDescriptor.put("collisionobject", CollisionObjectDesc.getDescriptor());
        extensionToDescriptor.put("gui", Gui.SceneDesc.getDescriptor());
        extensionToDescriptor.put("font", FontDesc.getDescriptor());
        extensionToDescriptor.put("material", MaterialDesc.getDescriptor());
        extensionToDescriptor.put("render", RenderPrototypeDesc.getDescriptor());
        extensionToDescriptor.put("input_binding", InputBinding.getDescriptor());
        extensionToDescriptor.put("collection", CollectionDesc.getDescriptor());

        extensionToTemplate.put("model", ModelDesc.newBuilder()
                .setName("unnamed")
                .setMesh("")
                .setMaterial("")
                .build());

        extensionToTemplate.put("go", GameObject.PrototypeDesc.newBuilder().build());

        extensionToTemplate.put("camera", CameraDesc.newBuilder()
            .setAspectRatio(1)
            .setFOV(45.0f)
            .setNearZ(0.1f)
            .setFarZ(1000.0f)
            .build());

        extensionToTemplate.put("light", LightDesc.newBuilder()
            .setId("unnamed")
            .setType(LightType.POINT)
            .setIntensity(10)
            .setColor(Vector3.newBuilder().setX(1).setY(1).setZ(1))
            .setRange(50)
            .setDecay(0.9f)
            .build());

        extensionToTemplate.put("spawnpoint", SpawnPointDesc.newBuilder()
            .setPrototype("")
            .build());

        extensionToTemplate.put("collisionobject", CollisionObjectDesc.newBuilder()
            .setCollisionShape("")
            .setType(CollisionObjectType.COLLISION_OBJECT_TYPE_DYNAMIC)
            .setMass(1)
            .setFriction(0.1f)
            .setRestitution(0.5f)
            .setGroup(1)
            .build());

        extensionToTemplate.put("gui", Gui.SceneDesc.newBuilder()
            .setScript("")
            .build());

        extensionToTemplate.put("font", FontDesc.newBuilder()
            .setFont("")
            .setMaterial("")
            .setSize(15)
            .build());

        extensionToTemplate.put("material", MaterialDesc.newBuilder()
            .setName("unnamed")
            .setVertexProgram("")
            .setFragmentProgram("")
            .build());

        extensionToTemplate.put("render", RenderPrototypeDesc.newBuilder()
            .setScript("")
            .build());

        extensionToTemplate.put("input", InputBinding.newBuilder().build());

        extensionToTemplate.put("collection", CollectionDesc.newBuilder()
            .setName("unnamed")
            .build());

        List<ComponentType> embeddedList = new ArrayList<ProtoFactory.ComponentType>(16);
        embeddedList.add(new ComponentType("Collision Object", "collisionobject"));

        embeddedComponentTypes = embeddedList.toArray(new ComponentType[embeddedList.size()]);
    }

    public static Descriptor getDescriptorForExtension(String extension) {
        return extensionToDescriptor.get(extension);
    }

    public static ComponentType[] getEmbeddedComponentTypes() {
        return embeddedComponentTypes;
    }

    public static Message getTemplateMessageForExtension(String extension) {
        return extensionToTemplate.get(extension);
    }

    public static Set<String> getTemplateMessageExtensions() {
        return extensionToTemplate.keySet();
    }

}

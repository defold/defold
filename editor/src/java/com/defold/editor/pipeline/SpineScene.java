package com.defold.editor.pipeline;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Vector;

import javax.vecmath.AxisAngle4d;
import javax.vecmath.Matrix4d;
import javax.vecmath.Point2d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector3d;

import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonParseException;
import org.codehaus.jackson.map.JsonMappingException;
import org.codehaus.jackson.map.ObjectMapper;

import com.defold.editor.pipeline.TextureSetGenerator.UVTransform;
import com.defold.editor.pipeline.SpineScene.AnimationTrack.Property;

/**
 * Convenience class for loading spine json data.
 *
 * Should preferably have been an extension to Bob rather than located inside it.
 */
public class SpineScene {
    @SuppressWarnings("serial")
    public static class LoadException extends Exception {
        public LoadException(String msg) {
            super(msg);
        }
    }

    public interface UVTransformProvider {
        UVTransform getUVTransform(String animId);
    }

    public static class Transform {
        public Point3d position = new Point3d();
        public Quat4d rotation = new Quat4d();
        public Vector3d scale = new Vector3d();

        public Transform() {
            this.position.set(0.0, 0.0, 0.0);
            this.rotation.set(0.0, 0.0, 0.0, 1.0);
            this.scale.set(1.0, 1.0, 1.0);
        }

        public Transform(Transform t) {
            set(t);
        }

        public void set(Transform t) {
            this.position.set(t.position);
            this.rotation.set(t.rotation);
            this.scale.set(t.scale);
        }

        public void setZAngleDeg(double angle) {
            double rads = angle * Math.PI / 180.0;
            this.rotation.set(new AxisAngle4d(new Vector3d(0.0, 0.0, 1.0), rads));
        }

        public void apply(Point3d p) {
            p.set(this.scale.x * p.x, this.scale.y * p.y, this.scale.z * p.z);
            MathUtil.rotate(this.rotation, p);
            p.add(this.position);
        }

        public void mul(Transform t) {
            // Scale and rotate position
            Point3d p = new Point3d(t.position);
            apply(p);
            this.position.set(p);
            // Intentionally non-rotated scale to avoid shearing in the transform
            this.scale.set(this.scale.x * t.scale.x, this.scale.y * t.scale.y, this.scale.z * t.scale.z);
            // Rotation
            this.rotation.mul(t.rotation);
        }

        public void inverse() {
            this.scale.set(1.0 / this.scale.x, 1.0 / this.scale.y, 1.0 / this.scale.z);
            this.rotation.conjugate();
            Point3d p = new Point3d(this.position);
            p.scale(-1.0);
            this.position.set(0.0, 0.0, 0.0);
            apply(p);
            this.position.set(p);
        }

        public void toMatrix4d(Matrix4d m) {
            m.set(this.rotation);
            m.setElement(0, 0, m.getElement(0, 0) * this.scale.getX());
            m.setElement(1, 1, m.getElement(1, 1) * this.scale.getY());
            m.setElement(2, 2, m.getElement(2, 2) * this.scale.getZ());
            m.setColumn(3, this.position.x, this.position.y, this.position.z, 1.0);
        }
    }

    public static class Bone {
        public String name = "";
        public Transform localT = new Transform();
        public Transform worldT = new Transform();
        public Transform invWorldT = new Transform();
        public Bone parent = null;
        public int index = -1;
        public boolean inheritScale = true;
    }

    public static class Mesh {
        public String attachment;
        public String path;
        public Slot slot;
        public boolean visible;
        // format is: x0, y0, z0, u0, v0, ...
        public float[] vertices;
        public int[] triangles;
        public float[] boneWeights;
        public int[] boneIndices;
        public float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};
    }

    public static class Slot {
        public String name;
        public Bone bone;
        public int index;
        public String attachment;
        public float[] color = new float[] {1.0f, 1.0f, 1.0f, 1.0f};

        public Slot(String name, Bone bone, int index, String attachment) {
            this.name = name;
            this.bone = bone;
            this.index = index;
            this.attachment = attachment;
        }
    }

    public static class AnimationCurve {
        public float x0;
        public float y0;
        public float x1;
        public float y1;
    }

    public static class AnimationKey {
        public float t;
        public float[] value = new float[4];
        public boolean stepped;
        public AnimationCurve curve;
    }

    public static abstract class AbstractAnimationTrack<Key extends AnimationKey> {
        public List<Key> keys = new ArrayList<Key>();
    }

    public static class AnimationTrack extends AbstractAnimationTrack<AnimationKey> {
        public enum Property {
            POSITION, ROTATION, SCALE
        }
        public Bone bone;
        public Property property;
    }

    public static class SlotAnimationKey extends AnimationKey {
        public String attachment;
        public int orderOffset;
    }

    public static class SlotAnimationTrack extends AbstractAnimationTrack<SlotAnimationKey> {
        public enum Property {
            ATTACHMENT, COLOR, DRAW_ORDER
        }
        public Slot slot;
        public Property property;
    }

    public static class Event {
        public String name;
        public String stringPayload;
        public float floatPayload;
        public int intPayload;
    }

    public static class EventKey {
        public float t;
        public String stringPayload;
        public float floatPayload;
        public int intPayload;
    }

    public static class EventTrack {
        public String name;
        public List<EventKey> keys = new ArrayList<EventKey>();
    }

    public static class Animation {
        public String name;
        public float duration;
        public List<AnimationTrack> tracks = new ArrayList<AnimationTrack>();
        public List<SlotAnimationTrack> slotTracks = new ArrayList<SlotAnimationTrack>();
        public List<EventTrack> eventTracks = new ArrayList<EventTrack>();
    }

    public List<Bone> bones = new ArrayList<Bone>();
    public Map<String, Bone> nameToBones = new HashMap<String, Bone>();
    public List<Mesh> meshes = new ArrayList<Mesh>();
    public Map<String, List<Mesh>> skins = new HashMap<String, List<Mesh>>();
    public Map<String, Animation> animations = new HashMap<String, Animation>();
    public Map<String, Slot> slots = new HashMap<String, Slot>();
    public Map<String, Event> events = new HashMap<String, Event>();

    public Bone getBone(String name) {
        return nameToBones.get(name);
    }

    public Bone getBone(int index) {
        return bones.get(index);
    }

    public Slot getSlot(String name) {
        return slots.get(name);
    }

    public Animation getAnimation(String name) {
        return animations.get(name);
    }

    public Event getEvent(String name) {
        return events.get(name);
    }

    private static void loadTransform(JsonNode node, Transform t) {
        t.position.set(JsonUtil.get(node, "x", 0.0), JsonUtil.get(node, "y", 0.0), 0.0);
        t.setZAngleDeg(JsonUtil.get(node, "rotation", 0.0));
        t.scale.set(JsonUtil.get(node, "scaleX", 1.0), JsonUtil.get(node, "scaleY", 1.0), 1.0);
    }

    private void loadBone(JsonNode boneNode) throws LoadException {
        Bone bone = new Bone();
        bone.name = boneNode.get("name").asText();
        bone.index = this.bones.size();
        if (boneNode.has("inheritScale")) {
            bone.inheritScale = boneNode.get("inheritScale").asBoolean();
        }
        loadTransform(boneNode, bone.localT);
        if (boneNode.has("parent")) {
            String parentName = boneNode.get("parent").asText();
            bone.parent = getBone(parentName);
            if (bone.parent == null) {
                throw new LoadException(String.format("The parent bone '%s' does not exist.", parentName));
            }
            bone.worldT.set(bone.parent.worldT);
            bone.worldT.mul(bone.localT);
            // Restore scale to local when it shouldn't be inherited
            if (!bone.inheritScale) {
                bone.worldT.scale.set(bone.localT.scale);
            }
        } else {
            bone.worldT.set(bone.localT);
        }
        bone.invWorldT.set(bone.worldT);
        bone.invWorldT.inverse();
        this.bones.add(bone);
        this.nameToBones.put(bone.name, bone);
    }

    private void loadRegion(JsonNode attNode, Mesh mesh, Bone bone) {
        Transform world = new Transform(bone.worldT);
        Transform local = new Transform();
        loadTransform(attNode, local);
        world.mul(local);
        int vertexCount = 4;
        mesh.vertices = new float[vertexCount * 5];
        mesh.boneIndices = new int[vertexCount * 4];
        mesh.boneWeights = new float[vertexCount * 4];
        double width = JsonUtil.get(attNode, "width", 0.0);
        double height = JsonUtil.get(attNode, "height", 0.0);
        double[] boundary = new double[] {-0.5, 0.5};
        double[] uv_boundary = new double[] {0.0, 1.0};

        String hex = JsonUtil.get(attNode, "color", "ffffffff");
        JsonUtil.hexToRGBA(hex, mesh.color);

        int i = 0;
        for (int xi = 0; xi < 2; ++xi) {
            for (int yi = 0; yi < 2; ++yi) {
                Point3d p = new Point3d(boundary[xi] * width, boundary[yi] * height, 0.0);
                world.apply(p);
                mesh.vertices[i++] = (float)p.x;
                mesh.vertices[i++] = (float)p.y;
                mesh.vertices[i++] = (float)p.z;
                mesh.vertices[i++] = (float)uv_boundary[xi];
                mesh.vertices[i++] = (float)(1.0 - uv_boundary[yi]);
                int boneOffset = (xi*2+yi)*4;
                mesh.boneIndices[boneOffset] = bone.index;
                mesh.boneWeights[boneOffset] = 1.0f;
            }
        }
        mesh.triangles = new int[] {
                0, 1, 2,
                2, 1, 3
        };
    }

    private class Weight implements Comparable<Weight> {
        public Point3d p;
        public int boneIndex;
        public float weight;

        public Weight(Point3d p, int boneIndex, float weight) {
            this.p = p;
            this.boneIndex = boneIndex;
            this.weight = weight;
        }

        private int toCompInt() {
            // weight is [0.0, 1.0]
            return (int)(Integer.MAX_VALUE * this.weight);
        }

        @Override
        public int compareTo(Weight o) {
            return o.toCompInt() - toCompInt();
        }
    }

    private void loadMesh(JsonNode attNode, Mesh mesh, Bone bone, boolean skinned) throws LoadException {
        Iterator<JsonNode> vertexIt = attNode.get("vertices").getElements();
        Iterator<JsonNode> uvIt = attNode.get("uvs").getElements();
        int vertexCount = 0;
        while (uvIt.hasNext()) {
            uvIt.next();
            uvIt.next();
            ++vertexCount;
        }
        uvIt = attNode.get("uvs").getElements();
        mesh.vertices = new float[vertexCount * 5];
        mesh.boneIndices = new int[vertexCount * 4];
        mesh.boneWeights = new float[vertexCount * 4];
        Vector<Weight> weights = new Vector<Weight>(10);
        Point3d p = new Point3d();
        for (int i = 0; i < vertexCount; ++i) {
            int boneOffset = i*4;
            weights.setSize(0);
            if (skinned) {
                int boneCount = vertexIt.next().asInt();
                p.set(0.0, 0.0, 0.0);
                for (int bi = 0; bi < boneCount; ++bi) {
                    int boneIndex = vertexIt.next().asInt();
                    double x = vertexIt.next().asDouble();
                    double y = vertexIt.next().asDouble();
                    double weight = vertexIt.next().asDouble();
                    if (weight > 0.0) {
                        weights.add(new Weight(new Point3d(x, y, 0.0), boneIndex, (float)weight));
                    }
                }
                if (weights.size() > 4) {
                    Collections.sort(weights);
                    weights.setSize(4);
                }
                float totalWeight = 0.0f;
                for (Weight w : weights) {
                    totalWeight += w.weight;
                }
                boneCount = weights.size();
                for (int bi = 0; bi < boneCount; ++bi) {
                    Weight w = weights.get(bi);
                    mesh.boneIndices[boneOffset+bi] = w.boneIndex;
                    mesh.boneWeights[boneOffset+bi] = w.weight / totalWeight;
                    Bone b = getBone(w.boneIndex);
                    b.worldT.apply(w.p);
                    w.p.scaleAdd(w.weight, p);
                    p = w.p;
                }
            } else {
                double x = vertexIt.next().asDouble();
                double y = vertexIt.next().asDouble();
                p.set(x, y, 0.0);
                bone.worldT.apply(p);
                mesh.boneIndices[boneOffset] = bone.index;
                mesh.boneWeights[boneOffset] = 1.0f;
            }
            int vi = i*5;
            mesh.vertices[vi++] = (float)p.x;
            mesh.vertices[vi++] = (float)p.y;
            mesh.vertices[vi++] = (float)p.z;
            mesh.vertices[vi++] = (float)uvIt.next().asDouble();
            mesh.vertices[vi++] = (float)uvIt.next().asDouble();
        }
        Iterator<JsonNode> triangleIt = attNode.get("triangles").getElements();
        List<Integer> triangles = new ArrayList<Integer>(vertexCount);
        while (triangleIt.hasNext()) {
            for (int i = 0; i < 3; ++i) {
                triangles.add(triangleIt.next().asInt());
            }
        }
        mesh.triangles = toPrimitive(triangles.toArray(new Integer[triangles.size()]));
    }

    private int[] toPrimitive(Integer[] ints) {
    	int[] result = new int[ints.length];
    	for (int i = 0; i < ints.length; ++i) {
    		result[i] = ints[i];
    	}
    	return result;
    }

    private void loadTrack(JsonNode propNode, AnimationTrack track) {
        // This value is used to counter how the key values for rotations are interpreted in spine:
        // * All values are modulated into the interval 0 <= x < 360
        // * If keys k0 and k1 have a difference > 180, the second key is adjusted with +/- 360 to lessen the difference to < 180
        // ** E.g. k0 = 0, k1 = 270 are interpolated as k0 = 0, k1 = -90
        Float prevAngles = null;
        Iterator<JsonNode> keyIt = propNode.getElements();
        while (keyIt.hasNext()) {
            JsonNode keyNode =  keyIt.next();
            AnimationKey key = new AnimationKey();
            key.t = (float)keyNode.get("time").asDouble();
            switch (track.property) {
            case POSITION:
                key.value = new float[] {JsonUtil.get(keyNode, "x", 0.0f), JsonUtil.get(keyNode, "y", 0.0f), 0.0f};
                break;
            case ROTATION:
                // See the comment above why this is done for rotations
                float angles = JsonUtil.get(keyNode, "angle", 0.0f);
                if (prevAngles != null) {
                    float diff = angles - prevAngles;
                    if (Math.abs(diff) > 180.0f) {
                        angles += 360.0f * -Math.signum(diff);
                    }
                }
                prevAngles = angles;
                key.value = new float[] {angles};
                break;
            case SCALE:
                key.value = new float[] {JsonUtil.get(keyNode, "x", 1.0f), JsonUtil.get(keyNode, "y", 1.0f), 1.0f};
                break;
            }
            if (keyNode.has("curve")) {
                JsonNode curveNode = keyNode.get("curve");
                if (curveNode.isArray()) {
                    AnimationCurve curve = new AnimationCurve();
                    Iterator<JsonNode> curveIt = curveNode.getElements();
                    curve.x0 = (float)curveIt.next().asDouble();
                    curve.y0 = (float)curveIt.next().asDouble();
                    curve.x1 = (float)curveIt.next().asDouble();
                    curve.y1 = (float)curveIt.next().asDouble();
                    key.curve = curve;
                } else if (curveNode.isTextual() && curveNode.asText().equals("stepped")) {
                    key.stepped = true;
                }
            }
            track.keys.add(key);
        }
    }

    private void loadSlotTrack(JsonNode propNode, SlotAnimationTrack track) {
        Iterator<JsonNode> keyIt = propNode.getElements();
        while (keyIt.hasNext()) {
            JsonNode keyNode =  keyIt.next();
            SlotAnimationKey key = new SlotAnimationKey();
            key.t = (float)keyNode.get("time").asDouble();
            switch (track.property) {
            case COLOR:
                // Hex to RGBA
                String hex = JsonUtil.get(keyNode, "color", "ffffffff");
                JsonUtil.hexToRGBA(hex, key.value);
                break;
            case ATTACHMENT:
                key.attachment = JsonUtil.get(keyNode, "name", (String)null);
                break;
            case DRAW_ORDER:
                // Handled separately, stored in separate JSON node
                break;
            }
            if (keyNode.has("curve")) {
                JsonNode curveNode = keyNode.get("curve");
                if (curveNode.isArray()) {
                    AnimationCurve curve = new AnimationCurve();
                    Iterator<JsonNode> curveIt = curveNode.getElements();
                    curve.x0 = (float)curveIt.next().asDouble();
                    curve.y0 = (float)curveIt.next().asDouble();
                    curve.x1 = (float)curveIt.next().asDouble();
                    curve.y1 = (float)curveIt.next().asDouble();
                    key.curve = curve;
                } else if (curveNode.isTextual() && curveNode.asText().equals("stepped")) {
                    key.stepped = true;
                }
            }
            track.keys.add(key);
        }
    }

    private void loadAnimation(JsonNode animNode, Animation animation) {
        JsonNode bonesNode = animNode.get("bones");
        if (bonesNode != null) {
            Iterator<Map.Entry<String, JsonNode>> animBoneIt = bonesNode.getFields();
            while (animBoneIt.hasNext()) {
                Map.Entry<String, JsonNode> boneEntry = animBoneIt.next();
                String boneName = boneEntry.getKey();
                JsonNode boneNode = boneEntry.getValue();
                Iterator<Map.Entry<String, JsonNode>> propIt = boneNode.getFields();
                while (propIt.hasNext()) {
                    Map.Entry<String, JsonNode> propEntry = propIt.next();
                    String propName = propEntry.getKey();
                    JsonNode propNode = propEntry.getValue();
                    AnimationTrack track = new AnimationTrack();
                    track.bone = getBone(boneName);
                    track.property = spineToProperty(propName);
                    loadTrack(propNode, track);
                    animation.tracks.add(track);
                }
            }
        }
        JsonNode slotsNode = animNode.get("slots");
        if (slotsNode != null) {
            Iterator<Map.Entry<String, JsonNode>> animSlotIt = slotsNode.getFields();
            while (animSlotIt.hasNext()) {
                Map.Entry<String, JsonNode> slotEntry = animSlotIt.next();
                String slotName = slotEntry.getKey();
                JsonNode slotNode = slotEntry.getValue();
                Iterator<Map.Entry<String, JsonNode>> propIt = slotNode.getFields();
                while (propIt.hasNext()) {
                    Map.Entry<String, JsonNode> propEntry = propIt.next();
                    String propName = propEntry.getKey();
                    JsonNode propNode = propEntry.getValue();
                    SlotAnimationTrack track = new SlotAnimationTrack();
                    track.slot = getSlot(slotName);
                    track.property = spineToSlotProperty(propName);
                    loadSlotTrack(propNode, track);
                    animation.slotTracks.add(track);
                }
            }
        }
        JsonNode eventsNode = animNode.get("events");
        if (eventsNode != null) {
            Iterator<JsonNode> keyIt = eventsNode.getElements();
            Map<String, List<EventKey>> tracks = new HashMap<String, List<EventKey>>();
            while (keyIt.hasNext()) {
                JsonNode keyNode = keyIt.next();
                String eventId = keyNode.get("name").asText();
                List<EventKey> keys = tracks.get(eventId);
                if (keys == null) {
                    keys = new ArrayList<EventKey>();
                    tracks.put(eventId, keys);
                }
                Event event = getEvent(eventId);
                EventKey key = new EventKey();
                key.t = (float)keyNode.get("time").asDouble();
                key.intPayload = JsonUtil.get(keyNode, "int", event.intPayload);
                key.floatPayload = JsonUtil.get(keyNode, "float", event.floatPayload);
                key.stringPayload = JsonUtil.get(keyNode, "string", event.stringPayload);
                keys.add(key);
            }
            for (Map.Entry<String, List<EventKey>> entry : tracks.entrySet()) {
                EventTrack track = new EventTrack();
                track.name = entry.getKey();
                track.keys = entry.getValue();
                animation.eventTracks.add(track);
            }
        }
        float duration = 0.0f;
        int signal_locked = 0x10CCED; // "LOCKED" indicates that draw order offset for this key should not be modified in runtime
        JsonNode drawOrderNode = animNode.get("drawOrder");
        if (drawOrderNode != null) {
            Iterator<JsonNode> animDrawOrderIt = drawOrderNode.getElements();
            Map<String, SlotAnimationTrack> slotTracks = new HashMap<String, SlotAnimationTrack>();

            while (animDrawOrderIt.hasNext()) {
                JsonNode drawOrderEntry = animDrawOrderIt.next();
                float t = JsonUtil.get(drawOrderEntry, "time", 0.0f);
                duration = Math.max(duration, t);
                JsonNode offsetsNode = drawOrderEntry.get("offsets");
                if (offsetsNode != null) {
                    Iterator<JsonNode> offsetIt = offsetsNode.getElements();
                    while (offsetIt.hasNext()) {
                        JsonNode offsetNode = offsetIt.next();
                        String slotName = JsonUtil.get(offsetNode, "slot", (String)null);
                        SlotAnimationTrack track = slotTracks.get(slotName);
                        if (track == null) {
                            track = new SlotAnimationTrack();
                            track.property = SlotAnimationTrack.Property.DRAW_ORDER;
                            track.slot = slots.get(slotName);
                            slotTracks.put(slotName, track);
                            animation.slotTracks.add(track);
                        }
                        SlotAnimationKey key = new SlotAnimationKey();
                        key.orderOffset = JsonUtil.get(offsetNode, "offset", 0);

                        // Key with explicit offset 0 means that a previous draw order offset is finished at this key, but it should yet not be considered unchanged.
                        if(key.orderOffset == 0) {
                            key.orderOffset = signal_locked;
                        }

                        key.t = t;
                        track.keys.add(key);
                    }
                }
                // Add default keys for all slots who were previously offset:ed but not explicitly changed in offset this key
                for (Map.Entry<String, SlotAnimationTrack> entry : slotTracks.entrySet()) {
                    SlotAnimationTrack track = entry.getValue();
                    SlotAnimationKey key = track.keys.get(track.keys.size() - 1);
                    if (key.t != t) {
                        key = new SlotAnimationKey();
                        key.orderOffset = 0;
                        key.t = t;
                        track.keys.add(key);
                    }
                }
            }
        }
        for (AnimationTrack track : animation.tracks) {
            for (AnimationKey key : track.keys) {
                duration = Math.max(duration, key.t);
            }
        }
        for (SlotAnimationTrack track : animation.slotTracks) {
            for (SlotAnimationKey key : track.keys) {
                duration = Math.max(duration, key.t);
            }
        }
        for (EventTrack track : animation.eventTracks) {
            for (EventKey key : track.keys) {
                duration = Math.max(duration, key.t);
            }
        }
        animation.duration = duration;
    }

    private void loadAnimations(JsonNode animationsNode) {
        Iterator<Map.Entry<String, JsonNode>> animationIt = animationsNode.getFields();
        while (animationIt.hasNext()) {
            Map.Entry<String, JsonNode> entry = animationIt.next();
            String animName = entry.getKey();
            JsonNode animNode = entry.getValue();
            Animation animation = new Animation();
            animation.name = animName;
            loadAnimation(animNode, animation);
            this.animations.put(animName, animation);
        }
    }

    public static SpineScene loadJson(InputStream is, UVTransformProvider uvTransformProvider) throws LoadException {
        SpineScene scene = new SpineScene();
        ObjectMapper m = new ObjectMapper();
        try {
            JsonNode node = m.readValue(new InputStreamReader(is, "UTF-8"), JsonNode.class);
            Iterator<JsonNode> boneIt = node.get("bones").getElements();
            while (boneIt.hasNext()) {
                JsonNode boneNode = boneIt.next();
                scene.loadBone(boneNode);
            }
            int slotIndex = 0;
            if (!node.has("slots")) {
                return scene;
            }
            Iterator<JsonNode> slotIt = node.get("slots").getElements();
            while (slotIt.hasNext()) {
                JsonNode slotNode = slotIt.next();
                String attachment = JsonUtil.get(slotNode, "attachment", (String)null);
                String boneName = slotNode.get("bone").asText();
                String slotName = JsonUtil.get(slotNode, "name", (String)null);
                Bone bone = scene.getBone(boneName);
                if (bone == null) {
                    throw new LoadException(String.format("The bone '%s' of attachment '%s' does not exist.", boneName, attachment));
                }
                Slot slot = new Slot(slotName, bone, slotIndex, attachment);
                JsonUtil.hexToRGBA(JsonUtil.get(slotNode,  "color",  "ffffffff"), slot.color);
                scene.slots.put(slotNode.get("name").asText(), slot);
                ++slotIndex;
            }
            if (node.has("events")) {
                Iterator<Map.Entry<String, JsonNode>> eventIt = node.get("events").getFields();
                while (eventIt.hasNext()) {
                    Map.Entry<String, JsonNode> eventEntry = eventIt.next();
                    Event event = new Event();
                    event.name = eventEntry.getKey();
                    JsonNode eventNode = eventEntry.getValue();
                    event.stringPayload = JsonUtil.get(eventNode, "string", "");
                    event.intPayload = JsonUtil.get(eventNode, "int", 0);
                    event.floatPayload = JsonUtil.get(eventNode, "float", 0.0f);
                    scene.events.put(event.name, event);
                }
            }
            if (!node.has("skins")) {
                return scene;
            }
            Iterator<Map.Entry<String, JsonNode>> skinIt = node.get("skins").getFields();
            while (skinIt.hasNext()) {
                Map.Entry<String, JsonNode> entry = skinIt.next();
                String skinName = entry.getKey();
                JsonNode skinNode = entry.getValue();
                Iterator<Map.Entry<String, JsonNode>> skinSlotIt = skinNode.getFields();
                List<Mesh> meshes = new ArrayList<Mesh>();
                while (skinSlotIt.hasNext()) {
                    Map.Entry<String, JsonNode> slotEntry = skinSlotIt.next();
                    String slotName = slotEntry.getKey();
                    Slot slot = scene.slots.get(slotName);
                    JsonNode slotNode = slotEntry.getValue();
                    Iterator<Map.Entry<String, JsonNode>> attIt = slotNode.getFields();
                    while (attIt.hasNext()) {
                        Map.Entry<String, JsonNode> attEntry = attIt.next();
                        String attName = attEntry.getKey();
                        JsonNode attNode = attEntry.getValue();
                        Bone bone = slot.bone;
                        if (bone == null) {
                            throw new LoadException(String.format("No bone mapped to attachment '%s'.", attName));
                        }
                        String path = attName;
                        if (attNode.has("name")) {
                            path = attNode.get("name").asText();
                        }
                        String type = JsonUtil.get(attNode, "type", "region");
                        Mesh mesh = new Mesh();
                        mesh.attachment = attName;
                        mesh.path = path;
                        mesh.slot = slot;
                        mesh.visible = attName.equals(slot.attachment);
                        if (type.equals("region")) {
                            scene.loadRegion(attNode, mesh, bone);
                        } else if (type.equals("mesh")) {
                            scene.loadMesh(attNode, mesh, bone, false);
                        } else if (type.equals("skinnedmesh") || type.equals("weightedmesh")) {
                            scene.loadMesh(attNode, mesh, bone, true);
                        } else {
                            mesh = null;
                        }
                        // Silently ignore unsupported types
                        if (mesh != null) {
                            transformUvs(mesh, uvTransformProvider);
                            meshes.add(mesh);
                        }
                    }
                }
                Collections.sort(meshes, new Comparator<Mesh>() {
                    @Override
                    public int compare(Mesh o1, Mesh o2) {
                        return o1.slot.index - o2.slot.index;
                    }
                });
                // Special handling of the default skin
                if (skinName.equals("default")) {
                    scene.meshes = meshes;
                } else {
                    scene.skins.put(skinName, meshes);
                }
            }
            if (!node.has("animations")) {
                return scene;
            }
            scene.loadAnimations(node.get("animations"));

            return scene;
        } catch (JsonParseException e) {
            throw new LoadException(e.getMessage());
        } catch (JsonMappingException e) {
            throw new LoadException(e.getMessage());
        } catch (UnsupportedEncodingException e) {
            throw new LoadException(e.getMessage());
        } catch (IOException e) {
            throw new LoadException(e.getMessage());
        }
    }

    private static void transformUvs(Mesh mesh, UVTransformProvider uvTransformProvider) throws LoadException {
        UVTransform t = uvTransformProvider.getUVTransform(mesh.path);
        if (t == null) {
            // default to identity
            t = new UVTransform();
        }
        int vertexCount = mesh.vertices.length / 5;
        Point2d p = new Point2d();
        for (int i = 0; i < vertexCount; ++i) {
            int uvi = i*5+3;
            p.x = mesh.vertices[uvi+0];
            p.y = mesh.vertices[uvi+1];
            t.apply(p);
            mesh.vertices[uvi+0] = (float)p.x;
            mesh.vertices[uvi+1] = (float)p.y;
        }
    }

    public static Property spineToProperty(String name) {
        if (name.equals("translate")) {
            return Property.POSITION;
        } else if (name.equals("rotate")) {
            return Property.ROTATION;
        } else if (name.equals("scale")) {
            return Property.SCALE;
        }
        return null;
    }

    public static SlotAnimationTrack.Property spineToSlotProperty(String name) {
        if (name.equals("color")) {
            return SlotAnimationTrack.Property.COLOR;
        } else if (name.equals("attachment")) {
            return SlotAnimationTrack.Property.ATTACHMENT;
        }
        return null;
    }

    public static class JsonUtil {

        public static double get(JsonNode n, String name, double defaultVal) {
            return n.has(name) ? n.get(name).asDouble() : defaultVal;
        }

        public static float get(JsonNode n, String name, float defaultVal) {
            return n.has(name) ? (float)n.get(name).asDouble() : defaultVal;
        }

        public static String get(JsonNode n, String name, String defaultVal) {
            return n.has(name) ? n.get(name).asText() : defaultVal;
        }

        public static int get(JsonNode n, String name, int defaultVal) {
            return n.has(name) ? n.get(name).asInt() : defaultVal;
        }

        public static void hexToRGBA(String hex, float[] value) {
            for (int i = 0; i < 4; ++i) {
                int offset = i*2;
                value[i] = Integer.valueOf(hex.substring(0 + offset, 2 + offset), 16) / 255.0f;
            }
        }
    }
}

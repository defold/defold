package com.dynamo.bob.util;

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

import javax.vecmath.Point2d;
import javax.vecmath.Point3d;

import org.apache.commons.lang.ArrayUtils;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonParseException;
import org.codehaus.jackson.map.JsonMappingException;
import org.codehaus.jackson.map.ObjectMapper;

import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.bob.util.RigUtil.Bone;
import com.dynamo.bob.util.RigUtil.MeshAttachment;
import com.dynamo.bob.util.RigUtil.SkinSlot;
import com.dynamo.bob.util.RigUtil.BaseSlot;
import com.dynamo.bob.util.RigUtil.IK;
import com.dynamo.bob.util.RigUtil.Animation;
import com.dynamo.bob.util.RigUtil.Event;
import com.dynamo.bob.util.RigUtil.Transform;
import com.dynamo.bob.util.RigUtil.AnimationTrack;
import com.dynamo.bob.util.RigUtil.AnimationTrack.Property;
import com.dynamo.bob.util.RigUtil.AnimationKey;
import com.dynamo.bob.util.RigUtil.AnimationCurve;
import com.dynamo.bob.util.RigUtil.IKAnimationTrack;
import com.dynamo.bob.util.RigUtil.IKAnimationKey;
import com.dynamo.bob.util.RigUtil.SlotAnimationTrack;
import com.dynamo.bob.util.RigUtil.SlotAnimationKey;
import com.dynamo.bob.util.RigUtil.EventTrack;
import com.dynamo.bob.util.RigUtil.EventKey;
import com.dynamo.bob.util.RigUtil.UVTransformProvider;
import com.dynamo.bob.util.RigUtil.Weight;

/**
 * Convenience class for loading spine json data.
 *
 * Should preferably have been an extension to Bob rather than located inside it.
 */
public class SpineSceneUtil {
    @SuppressWarnings("serial")
    public static class LoadException extends Exception {
        public LoadException(String msg) {
            super(msg);
        }
    }

    public static int slotSignalUnchanged = 0x10CCED;

    public String spineVersion = null;
    public boolean localBoneScaling = true;

    public List<Bone> bones = new ArrayList<Bone>();
    public List<IK> iks = new ArrayList<IK>();
    public Map<String, Bone> nameToBones = new HashMap<String, Bone>();
    public Map<String, IK> nameToIKs = new HashMap<String, IK>();

    // Slots
    public int slotCount = 0;
    public List<BaseSlot> baseSlots = new ArrayList<BaseSlot>();
    public Map<String, BaseSlot> baseSlotsLut = new HashMap<String, BaseSlot>();
    public List<MeshAttachment> attachments = new ArrayList<MeshAttachment>();
    public Map<Long, MeshAttachment> attachmentLut = new HashMap<Long, MeshAttachment>();
    // public Map<String, Integer> attachmentsLut = new HashMap<String, Integer>();

    // Skins
    public Map<String, List<SkinSlot>> skins = new HashMap<String, List<SkinSlot>>();
    public List<SkinSlot> defaultSkin = null;

    public Map<String, Animation> animations = new HashMap<String, Animation>();
    public Map<String, Event> events = new HashMap<String, Event>();

    public Bone getBone(String name) {
        return nameToBones.get(name);
    }

    public Bone getBone(int index) {
        return bones.get(index);
    }

    public IK getIK(String name) {
        return nameToIKs.get(name);
    }

    public IK getIK(int index) {
        return iks.get(index);
    }

    public BaseSlot getBaseSlot(String name) {
        return baseSlotsLut.get(name);
    }

    public Animation getAnimation(String name) {
        return animations.get(name);
    }

    public Event getEvent(String name) {
        return events.get(name);
    }

    public int getSlotCount() {
        return slotCount;
    }

    public List<SkinSlot> getDefaultSkin() {
        return defaultSkin;
    }

    public List<SkinSlot> getSkin(String name) {
        if (name.isEmpty()) {
            return getDefaultSkin();
        }
        return skins.get(name);
    }

    public List<MeshAttachment> getAttachmentsForSkin(String name) {
        List<SkinSlot> skin = getSkin(name);
        if (skin == null) {
            return new ArrayList<MeshAttachment>();
        }
        List<MeshAttachment> skinAttachments = new ArrayList<>();
        for (SkinSlot slot : skin) {
            if (slot.activeAttachment >= 0) {
                int meshAttachmentIndex = slot.meshAttachments.get(slot.activeAttachment);
                if (meshAttachmentIndex >= 0) {
                    MeshAttachment attachment = attachments.get(meshAttachmentIndex);
                    skinAttachments.add(attachment);
                }
            }
        }

        return skinAttachments;
    }

    public List<MeshAttachment> getDefaultAttachments() {
        return getAttachmentsForSkin("");
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
        if (boneNode.has("length")) {
            bone.length = boneNode.get("length").asDouble();
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

    private void loadIK(JsonNode ikNode) throws LoadException {
        IK ik = new IK();
        ik.name = JsonUtil.get(ikNode, "name", "unnamed");
        ik.index = this.iks.size();
        Iterator<JsonNode> bonesIt = ikNode.get("bones").getElements();
        if (bonesIt.hasNext()) {
            String childBone = bonesIt.next().asText();
            ik.child = getBone(childBone);
        }
        if (bonesIt.hasNext()) {
            ik.parent = ik.child;
            String childBone = bonesIt.next().asText();
            ik.child = getBone(childBone);
        }
        String targetName = JsonUtil.get(ikNode, "target", (String)null);
        Bone target = getBone(targetName);
        if (targetName == null || bones == null) {
            throw new LoadException(String.format("The IK '%s' has an invalid target", ik.name));
        }
        ik.target = target;
        ik.positive = JsonUtil.get(ikNode, "bendPositive", true);
        ik.mix = JsonUtil.get(ikNode, "mix", 1.0f);
        iks.add(ik);
        this.nameToIKs.put(ik.name, ik);
    }

    private void loadRegion(JsonNode attNode, MeshAttachment mesh, Bone bone) {
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

        mesh.hash();
    }

    private void loadMesh(JsonNode attNode, MeshAttachment mesh, Bone bone, boolean skinned) throws LoadException {
        String hex = JsonUtil.get(attNode, "color", "ffffffff");
        JsonUtil.hexToRGBA(hex, mesh.color);

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
        mesh.triangles = ArrayUtils.toPrimitive(triangles.toArray(new Integer[triangles.size()]));

        mesh.hash();
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
            key.t = keyNode.get("time").asDouble();
            switch (track.property) {
            case POSITION:
                key.value = new float[] {JsonUtil.get(keyNode, "x", 0.0f), JsonUtil.get(keyNode, "y", 0.0f), 0.0f};
                break;
            case ROTATION:
                // See the comment above why this is done for rotations
                float angles = JsonUtil.get(keyNode, "angle", 0.0f);
                angles = angles % 360.0f;
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

    private void loadIKTrack(JsonNode propNode, IKAnimationTrack track) {
        Iterator<JsonNode> keyIt = propNode.getElements();
        while (keyIt.hasNext()) {
            JsonNode keyNode =  keyIt.next();
            IKAnimationKey key = new IKAnimationKey();
            key.t = keyNode.get("time").asDouble();
            key.mix = (float)JsonUtil.get(keyNode, "mix", 1.0f);
            key.positive = JsonUtil.get(keyNode, "bendPositive", true);
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
            key.t = keyNode.get("time").asDouble();
            switch (track.property) {
            case COLOR:
                // Hex to RGBA
                String hex = JsonUtil.get(keyNode, "color", "ffffffff");
                JsonUtil.hexToRGBA(hex, key.value);
                break;
            case ATTACHMENT:
                BaseSlot baseSlot = baseSlots.get(track.slot);
                String attachmentPointName = JsonUtil.get(keyNode, "name", null);
                if (attachmentPointName != null) {
                    Integer attachmentPointIndex = baseSlot.attachmentsLut.get(attachmentPointName);
                    if (attachmentPointIndex == null) {
                        // If the attachment name wasn't found it means that no skin
                        // has an actual mesh for this attachment, effectively it will never be drawn.
                        key.attachment = -1;
                    } else {
                        key.attachment = attachmentPointIndex;
                    }
                } else {
                    key.attachment = -1;
                }
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
                    Property prop = spineToProperty(propName);
                    if (prop == null) {
                        continue;
                    }
                    JsonNode propNode = propEntry.getValue();
                    AnimationTrack track = new AnimationTrack();
                    track.bone = getBone(boneName);
                    track.property = prop;
                    loadTrack(propNode, track);
                    animation.tracks.add(track);
                }
            }
        }

        JsonNode iKsNode = animNode.get("ik");
        if (iKsNode != null) {
            Iterator<Map.Entry<String, JsonNode>> animIkIt = iKsNode.getFields();
            while (animIkIt.hasNext()) {
                Map.Entry<String, JsonNode> iKEntry = animIkIt.next();
                String iKName = iKEntry.getKey();
                JsonNode iKNode = iKEntry.getValue();
                IKAnimationTrack track = new IKAnimationTrack();
                track.ik = getIK(iKName);
                loadIKTrack(iKNode, track);
                animation.iKTracks.add(track);
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
                    SlotAnimationTrack.Property prop = spineToSlotProperty(propName);
                    if (prop == null) {
                        continue;
                    }
                    JsonNode propNode = propEntry.getValue();
                    SlotAnimationTrack track = new SlotAnimationTrack();
                    BaseSlot slot = getBaseSlot(slotName);
                    track.slot = slot.index;
                    track.property = prop;
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
                key.t = keyNode.get("time").asDouble();
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
        double duration = 0.0f;
        JsonNode drawOrderNode = animNode.get("drawOrder");
        if (drawOrderNode != null) {
            Iterator<JsonNode> animDrawOrderIt = drawOrderNode.getElements();
            Map<String, SlotAnimationTrack> slotTracks = new HashMap<String, SlotAnimationTrack>();

            while (animDrawOrderIt.hasNext()) {
                JsonNode drawOrderEntry = animDrawOrderIt.next();
                double t = JsonUtil.get(drawOrderEntry, "time", 0.0f);
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
                            BaseSlot slot = baseSlotsLut.get(slotName);
                            track.slot = slot.index;
                            slotTracks.put(slotName, track);
                            animation.slotTracks.add(track);
                        }
                        SlotAnimationKey key = new SlotAnimationKey();
                        key.orderOffset = JsonUtil.get(offsetNode, "offset", 0);
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
                        key.orderOffset = slotSignalUnchanged;
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
        for (IKAnimationTrack track : animation.iKTracks) {
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

    private static long getIteratorSize(Iterator<?> iterator) {
        long size = 0;
        while (iterator.hasNext()) {
            size += 1;
            iterator.next();
        }

        return size;
    }

    private static List<SkinSlot> loadSkin(SpineSceneUtil scene, String skinName, JsonNode skinNode, UVTransformProvider uvTransformProvider) throws LoadException {
        Iterator<Map.Entry<String, JsonNode>> skinSlotIt = skinNode.getFields();

        Map<String, SkinSlot> skinSlotsLut = new HashMap<String, SkinSlot>();
        List<SkinSlot> skinSlots = new ArrayList<SkinSlot>();

        boolean isDefaultSkin = false;
        List<SkinSlot> defaultSkin = scene.getDefaultSkin();
        if (defaultSkin == null) {
            // This is the default skin, create list of SkinSlots from BaseSlotss
            for (BaseSlot baseSlot : scene.baseSlots) {
                SkinSlot skinSlot = new SkinSlot(baseSlot);
                skinSlotsLut.put(baseSlot.name, skinSlot);
                skinSlots.add(skinSlot);
            }

            isDefaultSkin = true;
        } else {
            // Copy default skin
            for (SkinSlot defaultSlot : defaultSkin) {
                SkinSlot skinSlot = new SkinSlot(defaultSlot);
                skinSlotsLut.put(defaultSlot.baseSlot.name, skinSlot);
                skinSlots.add(skinSlot);
            }
        }

        while (skinSlotIt.hasNext()) {
            Map.Entry<String, JsonNode> slotEntry = skinSlotIt.next();
            String slotName = slotEntry.getKey();
            SkinSlot slot = skinSlotsLut.get(slotName);
            JsonNode slotNode = slotEntry.getValue();

            Iterator<Map.Entry<String, JsonNode>> attIt = slotNode.getFields();
            while (attIt.hasNext()) {
                Map.Entry<String, JsonNode> attEntry = attIt.next();
                String attName = attEntry.getKey();
                JsonNode attNode = attEntry.getValue();

                String path = attName;
                if (attNode.has("name")) {
                    path = attNode.get("name").asText();
                }

                Bone bone = slot.baseSlot.bone;
                if (bone == null) {
                    throw new LoadException(String.format("No bone mapped to attachment '%s'.", attName));
                }

                String type = JsonUtil.get(attNode, "type", "region");
                MeshAttachment mesh = new MeshAttachment();
                mesh.path = path;

                if ((scene.spineVersion != null) && (scene.spineVersion.compareTo("3.3.07") >= 0)) {
                    // spineVersion >= 3.3.07
                    if (type.equals("region")) {
                        scene.loadRegion(attNode, mesh, bone);
                    } else if (type.equals("mesh")) {
                        // For each vertex either an x,y pair or, for a weighted mesh, first
                        // the number of bones which influence the vertex, then for that
                        // many bones: bone index, bind position X, bind position Y, weight.
                        // A mesh is weighted if the number of vertices > number of UVs.
                        // http://esotericsoftware.com/spine-json-format
                        long verticesSize = getIteratorSize(attNode.get("vertices").getElements());
                        long uvSize = getIteratorSize(attNode.get("uvs").getElements());
                        if (verticesSize > uvSize) {
                            scene.loadMesh(attNode, mesh, bone, true);
                        } else {
                            scene.loadMesh(attNode, mesh, bone, false);
                        }
                    } else {
                        mesh = null;
                    }
                } else {
                    if (type.equals("region")) {
                        scene.loadRegion(attNode, mesh, bone);
                    } else if (type.equals("mesh")) {
                        scene.loadMesh(attNode, mesh, bone, false);
                    } else if (type.equals("skinnedmesh") || type.equals("weightedmesh")) {
                        scene.loadMesh(attNode, mesh, bone, true);
                    } else {
                        mesh = null;
                    }
                }

                // Silently ignore unsupported types
                if (mesh != null) {
                    transformUvs(mesh, uvTransformProvider);

                    // Check if mesh already has been found
                    int attachmentMeshIndex = scene.attachments.size();
                    if (scene.attachmentLut.containsKey(mesh.uniqueHash))
                    {
                        MeshAttachment prev = scene.attachmentLut.get(mesh.uniqueHash);
                        attachmentMeshIndex = prev.index;
                    } else {
                        // Add mesh data to scene global list
                        mesh.index = attachmentMeshIndex;
                        scene.attachments.add(mesh);
                        scene.attachmentLut.put(mesh.uniqueHash, mesh);
                    }


                    // Figure out to what attachment point this mesh is attached
                    int attachmentPointIndex = slot.baseSlot.attachmentsLut.get(attName);
                    slot.meshAttachments.set(attachmentPointIndex, attachmentMeshIndex);
                }
            }
        }

        return skinSlots;
    }

    private static void getAllAttachments(JsonNode skinJsonNode, SpineSceneUtil scene) throws LoadException {

        Iterator<Map.Entry<String, JsonNode>> skinIt = skinJsonNode.getFields();

        // Loop over all skin entries from JSON
        while (skinIt.hasNext()) {
            Map.Entry<String, JsonNode> entry = skinIt.next();
            String skinName = entry.getKey();
            JsonNode skinNode = entry.getValue();

            // Loop over all slots in the skin
            Iterator<Map.Entry<String, JsonNode>> skinSlotIt = skinNode.getFields();
            while (skinSlotIt.hasNext()) {
                Map.Entry<String, JsonNode> slotEntry = skinSlotIt.next();
                String slotName = slotEntry.getKey();
                JsonNode slotNode = slotEntry.getValue();

                // Get corresponding BaseSlot
                BaseSlot slot = scene.getBaseSlot(slotName);

                // Collect all attachments this skin can attach to this slot
                Iterator<Map.Entry<String, JsonNode>> attIt = slotNode.getFields();
                while (attIt.hasNext()) {
                    Map.Entry<String, JsonNode> attEntry = attIt.next();
                    String attName = attEntry.getKey();

                    if (!slot.attachmentsLut.containsKey(attName)) {
                        slot.attachmentsLut.put(attName, slot.attachments.size());
                        slot.attachments.add(attName);
                    }
                }
            }
        }

        // Apply default attachments based on original slot JSON list
        for (int i = 0; i < scene.baseSlots.size(); i++) {
            BaseSlot baseSlot = scene.baseSlots.get(i);
            if (baseSlot.defaultAttachmentString != null && !baseSlot.defaultAttachmentString.isEmpty()) {
                baseSlot.activeAttachment = baseSlot.attachmentsLut.get(baseSlot.defaultAttachmentString);
            }
        }
    }

    public static SpineSceneUtil loadJson(InputStream is, UVTransformProvider uvTransformProvider) throws LoadException {
        SpineSceneUtil scene = new SpineSceneUtil();
        ObjectMapper m = new ObjectMapper();
        try {
            JsonNode node = m.readValue(new InputStreamReader(is, "UTF-8"), JsonNode.class);
            Iterator<JsonNode> boneIt = node.get("bones").getElements();
            while (boneIt.hasNext()) {
                JsonNode boneNode = boneIt.next();
                scene.loadBone(boneNode);
            }
            JsonNode ikNode = node.get("ik");
            if (ikNode != null) {
                Iterator<JsonNode> ikIt = ikNode.getElements();
                while (ikIt.hasNext()) {
                    scene.loadIK(ikIt.next());
                }
            }
            if (!node.has("slots")) {
                return scene;
            }
            Iterator<JsonNode> slotIt = node.get("slots").getElements();
            int slotIndex = 0;
            int slotCount = 0;
            while (slotIt.hasNext()) {
                JsonNode slotNode = slotIt.next();
                String attachment = JsonUtil.get(slotNode, "attachment", (String)null);
                String boneName = slotNode.get("bone").asText();
                String slotName = JsonUtil.get(slotNode, "name", (String)null);
                Bone bone = scene.getBone(boneName);
                if (bone == null) {
                    throw new LoadException(String.format("The bone '%s' of attachment '%s' does not exist.", boneName, attachment));
                }

                BaseSlot slot = new BaseSlot(slotName, slotIndex, bone, attachment);
                JsonUtil.hexToRGBA(JsonUtil.get(slotNode,  "color",  "ffffffff"), slot.color);
                scene.baseSlots.add(slot);
                scene.baseSlotsLut.put(slotName, slot);

                ++slotIndex;
                ++slotCount;
            }
            scene.slotCount = slotCount;

            // Gather all available attachment points in all slots.
            // We do this by looping over all the different skins and their slots.
            getAllAttachments(node.get("skins"), scene);

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

            JsonNode skeleton = node.get("skeleton");
            scene.spineVersion = (skeleton != null) ? JsonUtil.get(skeleton, "spine", (String) null) : null;

            // If Spine version is 3 and above it uses a different scaling model than 2.x.
            if (scene.spineVersion != null) {
                String[] versionParts = scene.spineVersion.split("\\.");
                if (versionParts != null && Integer.parseInt(versionParts[0]) >= 3) {
                    scene.localBoneScaling = false;
                }
            }

            // Load default skin first since other skins will be based on this.
            JsonNode defaultSkinNode = node.get("skins").get("default");
            scene.defaultSkin = loadSkin(scene, "default", defaultSkinNode, uvTransformProvider);

            Iterator<Map.Entry<String, JsonNode>> skinIt = node.get("skins").getFields();
            while (skinIt.hasNext()) {
                Map.Entry<String, JsonNode> entry = skinIt.next();
                String skinName = entry.getKey();
                JsonNode skinNode = entry.getValue();
                if (!skinName.equals("default"))
                {
                    List<SkinSlot> skin = loadSkin(scene, skinName, skinNode, uvTransformProvider);
                    scene.skins.put(skinName, skin);
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

    private static void transformUvs(MeshAttachment mesh, UVTransformProvider uvTransformProvider) throws LoadException {
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
            return n.has(name) ? (!n.get(name).isNull() ? n.get(name).asText() : defaultVal) : defaultVal;
        }

        public static int get(JsonNode n, String name, int defaultVal) {
            return n.has(name) ? n.get(name).asInt() : defaultVal;
        }

        public static boolean get(JsonNode n, String name, boolean defaultVal) {
            return n.has(name) ? n.get(name).asBoolean() : defaultVal;
        }

        public static void hexToRGBA(String hex, float[] value) {
            for (int i = 0; i < 4; ++i) {
                int offset = i*2;
                value[i] = Integer.valueOf(hex.substring(0 + offset, 2 + offset), 16) / 255.0f;
            }
        }
    }
}

package com.dynamo.bob.pipeline;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.vecmath.Point2d;
import javax.vecmath.Point3d;
import javax.vecmath.Quat4d;
import javax.vecmath.Vector2d;
import javax.vecmath.Vector3d;

import com.dynamo.bob.Builder;
import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.textureset.TextureSetGenerator.UVTransform;
import com.dynamo.bob.util.MathUtil;
import com.dynamo.bob.util.MurmurHash;
import com.dynamo.bob.util.SpineSceneUtil;
import com.dynamo.bob.util.SpineSceneUtil.LoadException;
import com.dynamo.bob.util.RigUtil;
import com.dynamo.bob.util.RigUtil.UVTransformProvider;
import com.dynamo.spine.proto.Spine.SpineSceneDesc;
import com.dynamo.rig.proto.Rig;
import com.dynamo.rig.proto.Rig.AnimationSet;
import com.dynamo.rig.proto.Rig.AnimationTrack;
import com.dynamo.rig.proto.Rig.IKAnimationTrack;
import com.dynamo.rig.proto.Rig.Bone;
import com.dynamo.rig.proto.Rig.EventKey;
import com.dynamo.rig.proto.Rig.EventTrack;
import com.dynamo.rig.proto.Rig.IK;
import com.dynamo.rig.proto.Rig.Mesh;
import com.dynamo.rig.proto.Rig.MeshAnimationTrack;
import com.dynamo.rig.proto.Rig.MeshEntry;
import com.dynamo.rig.proto.Rig.MeshSet;
import com.dynamo.rig.proto.Rig.Skeleton;
import com.dynamo.rig.proto.Rig.RigAnimation;
import com.dynamo.textureset.proto.TextureSetProto.TextureSet;
import com.dynamo.textureset.proto.TextureSetProto.TextureSetAnimation;
import com.google.protobuf.Message;

@ProtoParams(messageClass = SpineSceneDesc.class)
@BuilderParams(name="SpineScene", inExts=".spinescene", outExt=".rigscenec")
public class SpineSceneBuilder extends Builder<Void> {

    @Override
    public Task<Void> create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder<Void> taskBuilder = Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()));

        SpineSceneDesc.Builder builder = SpineSceneDesc.newBuilder();
        ProtoUtil.merge(input, builder);
        BuilderUtil.checkResource(this.project, input, "spine_json", builder.getSpineJson());
        BuilderUtil.checkResource(this.project, input, "atlas", builder.getAtlas());

        taskBuilder.addOutput(input.changeExt(".skeletonc"));
        taskBuilder.addOutput(input.changeExt(".meshsetc"));
        taskBuilder.addOutput(input.changeExt(".animationsetc"));

        taskBuilder.addInput(input.getResource(builder.getSpineJson()));
        taskBuilder.addInput(project.getResource(project.getBuildDirectory() + BuilderUtil.replaceExt( builder.getAtlas(), "atlas", "texturesetc")));

        for (String t : builder.getTexturesList()) {
            IResource r = BuilderUtil.checkResource(this.project, input, "texture", t);
            taskBuilder.addInput(r);
        }

        return taskBuilder.build();
    }

    private static int reindexNodesDepthFirst(RigUtil.Bone bone, Map<RigUtil.Bone, List<RigUtil.Bone>> children, int index) {
        List<RigUtil.Bone> c = children.get(bone);
        if (c != null) {
            for (RigUtil.Bone child : c) {
                child.index = index++;
                index = reindexNodesDepthFirst(child, children, index);
            }
        }
        return index;
    }

    private static List<Integer> toDDF(List<RigUtil.Bone> bones, List<RigUtil.IK> iks, Skeleton.Builder skeletonBuilder) {
        // Order bones strictly breadth-first
        Map<RigUtil.Bone, List<RigUtil.Bone>> children = new HashMap<RigUtil.Bone, List<RigUtil.Bone>>();
        for (RigUtil.Bone bone : bones) {
            if (bone.parent != null) {
                List<RigUtil.Bone> c = children.get(bone.parent);
                if (c == null) {
                    c = new ArrayList<RigUtil.Bone>();
                    children.put(bone.parent, c);
                }
                c.add(bone);
            }
        }
        reindexNodesDepthFirst(bones.get(0), children, 1);
        List<Integer> indexRemap = new ArrayList<Integer>(bones.size());
        for (int i = 0; i < bones.size(); ++i) {
            indexRemap.add(bones.get(i).index);
        }
        Collections.sort(bones, new Comparator<RigUtil.Bone>() {
            @Override
            public int compare(RigUtil.Bone o1, RigUtil.Bone o2) {
                return o1.index - o2.index;
            }
        });
        for (RigUtil.Bone bone : bones) {
            Bone.Builder boneBuilder = Bone.newBuilder();
            boneBuilder.setId(MurmurHash.hash64(bone.name));
            int parentIndex = 0xffff;
            if (bone.parent != null) {
                parentIndex = bone.parent.index;
            }
            boneBuilder.setParent(parentIndex);
            boneBuilder.setPosition(MathUtil.vecmathToDDF(bone.localT.position));
            boneBuilder.setRotation(MathUtil.vecmathToDDF(bone.localT.rotation));
            boneBuilder.setScale(MathUtil.vecmathToDDF(bone.localT.scale));
            boneBuilder.setInheritScale(bone.inheritScale);
            boneBuilder.setLength((float)bone.length);
            skeletonBuilder.addBones(boneBuilder);
        }
        for (RigUtil.IK ik : iks) {
            IK.Builder ikBuilder = IK.newBuilder();
            ikBuilder.setId(MurmurHash.hash64(ik.name));
            if (ik.parent != null) {
                ikBuilder.setParent(ik.parent.index);
            } else {
                ikBuilder.setParent(ik.child.index);
            }
            ikBuilder.setChild(ik.child.index);
            ikBuilder.setTarget(ik.target.index);
            ikBuilder.setPositive(ik.positive);
            ikBuilder.setMix(ik.mix);
            skeletonBuilder.addIks(ikBuilder);
        }
        return indexRemap;
    }

    private static void toDDF(RigUtil.Mesh mesh, Mesh.Builder meshBuilder, List<Integer> boneIndexRemap, int drawOrder) {
        float[] v = mesh.vertices;
        int vertexCount = v.length / 5;
        for (int i = 0; i < vertexCount; ++i) {
            int vi = i * 5;
            for (int pi = 0; pi < 3; ++pi) {
                meshBuilder.addPositions(v[vi+pi]);
            }
            for (int ti = 3; ti < 5; ++ti) {
                meshBuilder.addTexcoord0(v[vi+ti]);
            }
        }
        for (int ci = 0; ci < 4; ++ci) {
            meshBuilder.addColor(mesh.slot.color[ci]);
            meshBuilder.addSkinColor(mesh.color[ci]);
        }
        if (mesh.boneIndices != null) {
            for (int boneIndex : mesh.boneIndices) {
                meshBuilder.addBoneIndices(boneIndexRemap.get(boneIndex));
            }
            for (float boneWeight : mesh.boneWeights) {
                meshBuilder.addWeights(boneWeight);
            }
        }
        for (int index : mesh.triangles) {
            meshBuilder.addIndices(index);
        }
        meshBuilder.setVisible(mesh.visible);
        meshBuilder.setDrawOrder(drawOrder);
    }

    private static List<MeshIndex> getIndexList(Map<String, List<MeshIndex>> slotIndices, String slot) {
        List<MeshIndex> indexList = slotIndices.get(slot);
        if (indexList == null) {
            indexList = new ArrayList<MeshIndex>();
            slotIndices.put(slot, indexList);
        }
        return indexList;
    }

    private static Map<String, List<MeshIndex>> toDDF(String skinName, List<RigUtil.Mesh> generics, List<RigUtil.Mesh> specifics, MeshSet.Builder meshSetBuilder, List<Integer> boneIndexRemap) {
        Map<String, List<MeshIndex>> slotIndices = new HashMap<String, List<MeshIndex>>();
        MeshEntry.Builder meshEntryBuilder = MeshEntry.newBuilder();
        meshEntryBuilder.setId(MurmurHash.hash64(skinName));
        List<RigUtil.Mesh> meshes = new ArrayList<RigUtil.Mesh>(generics.size() + specifics.size());
        meshes.addAll(generics);
        meshes.addAll(specifics);
        Collections.sort(meshes, new Comparator<RigUtil.Mesh>() {
            @Override
            public int compare(com.dynamo.bob.util.RigUtil.Mesh arg0,
                    com.dynamo.bob.util.RigUtil.Mesh arg1) {
                return arg0.slot.index - arg1.slot.index;
            }
        });
        int meshIndex = 0;
        for (RigUtil.Mesh mesh : meshes) {
            Mesh.Builder meshBuilder = Mesh.newBuilder();
            toDDF(mesh, meshBuilder, boneIndexRemap, mesh.slot.index);
            meshEntryBuilder.addMeshes(meshBuilder);
            List<MeshIndex> indexList = getIndexList(slotIndices, mesh.slot.name);
            indexList.add(new MeshIndex(meshIndex, mesh.attachment));
            ++meshIndex;
        }
        meshSetBuilder.addMeshEntries(meshEntryBuilder);
        return slotIndices;
    }

    private static void toDDF(RigUtil.AnimationTrack track, AnimationTrack.Builder animTrackBuilder, double duration, double sampleRate, double spf) {
        switch (track.property) {
        case POSITION:
            RigUtil.PositionBuilder posBuilder = new RigUtil.PositionBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, posBuilder, new Point3d(0.0, 0.0, 0.0), 0.0, duration, sampleRate, spf, true, false);
            break;
        case ROTATION:
            RigUtil.RotationBuilder rotBuilder = new RigUtil.RotationBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, rotBuilder, new Quat4d(0.0, 0.0, 0.0, 1.0), 0.0, duration, sampleRate, spf, true, false);
            break;
        case SCALE:
            RigUtil.ScaleBuilder scaleBuilder = new RigUtil.ScaleBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, scaleBuilder, new Vector3d(1.0, 1.0, 1.0), 0.0, duration, sampleRate, spf, true, false);
            break;
        }
    }

    private static void toDDF(RigUtil.IKAnimationTrack track, IKAnimationTrack.Builder iKanimTrackBuilder, double duration, double sampleRate, double spf) {
        RigUtil.IKMixBuilder mixBuilder = new RigUtil.IKMixBuilder(iKanimTrackBuilder);
        RigUtil.sampleTrack(track, mixBuilder, track.ik.mix, 0.0, duration, sampleRate, spf, false, false);
        RigUtil.IKPositiveBuilder positiveBuilder = new RigUtil.IKPositiveBuilder(iKanimTrackBuilder);
        RigUtil.sampleTrack(track, positiveBuilder, track.ik.positive, 0.0, duration, sampleRate, spf, false, false);
    }

    private static void toDDF(RigUtil.Slot slot, RigUtil.SlotAnimationTrack track, MeshAnimationTrack.Builder animTrackBuilder, double duration, double sampleRate, double spf, String meshName) {
        switch (track.property) {
        case ATTACHMENT:
            RigUtil.VisibilityBuilder visibilityBuilder = new RigUtil.VisibilityBuilder(animTrackBuilder, meshName);
            RigUtil.sampleTrack(track, visibilityBuilder, new Boolean(meshName.equals(slot.attachment)), 0.0, duration, sampleRate, spf, false, false);
            break;
        case COLOR:
            RigUtil.ColorBuilder colorBuilder = new RigUtil.ColorBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, colorBuilder, slot.color, 0.0, duration, sampleRate, spf, true, false);
            break;
        case DRAW_ORDER:
            RigUtil.DrawOrderBuilder drawOrderBuilder = new RigUtil.DrawOrderBuilder(animTrackBuilder);
            RigUtil.sampleTrack(track, drawOrderBuilder, new Integer(0), 0.0, duration, sampleRate, spf, false, false);
            break;
        }
    }

    private static void toDDF(RigUtil.EventTrack track, EventTrack.Builder builder) {
        builder.setEventId(MurmurHash.hash64(track.name));
        for (RigUtil.EventKey key : track.keys) {
            EventKey.Builder keyBuilder = EventKey.newBuilder();
            keyBuilder.setT(key.t).setInteger(key.intPayload).setFloat(key.floatPayload).setString(MurmurHash.hash64(key.stringPayload));
            builder.addKeys(keyBuilder);
        }
    }

    private static void toDDF(SpineSceneUtil scene, String id, RigUtil.Animation animation, AnimationSet.Builder animSetBuilder, double sampleRate, Map<Long, Map<String, List<MeshIndex>>> slotIndices) {
        RigAnimation.Builder animBuilder = RigAnimation.newBuilder();
        animBuilder.setId(MurmurHash.hash64(id));
        animBuilder.setDuration(animation.duration);
        animBuilder.setSampleRate((float)sampleRate);
        double spf = 1.0 / sampleRate;
        if (!animation.tracks.isEmpty()) {
            List<AnimationTrack.Builder> builders = new ArrayList<AnimationTrack.Builder>();
            AnimationTrack.Builder animTrackBuilder = AnimationTrack.newBuilder();
            RigUtil.AnimationTrack firstTrack = animation.tracks.get(0);
            animTrackBuilder.setBoneIndex(firstTrack.bone.index);
            for (RigUtil.AnimationTrack track : animation.tracks) {
                if (animTrackBuilder.getBoneIndex() != track.bone.index) {
                    builders.add(animTrackBuilder);
                    animTrackBuilder = AnimationTrack.newBuilder();
                    animTrackBuilder.setBoneIndex(track.bone.index);
                }
                toDDF(track, animTrackBuilder, animation.duration, sampleRate, spf);
            }
            builders.add(animTrackBuilder);
            // Compiled anim tracks must be in bone order
            Collections.sort(builders, new Comparator<AnimationTrack.Builder>() {
                @Override
                public int compare(AnimationTrack.Builder o1, AnimationTrack.Builder o2) {
                    return o1.getBoneIndex() - o2.getBoneIndex();
                }
            });
            for (AnimationTrack.Builder builder : builders) {
                animBuilder.addTracks(builder);
            }
        }

        if (!animation.iKTracks.isEmpty()) {
            List<IKAnimationTrack.Builder> builders = new ArrayList<IKAnimationTrack.Builder>();
            IKAnimationTrack.Builder animTrackBuilder = IKAnimationTrack.newBuilder();
            RigUtil.IKAnimationTrack firstTrack = animation.iKTracks.get(0);
            animTrackBuilder.setIkIndex(firstTrack.ik.index);
            for (RigUtil.IKAnimationTrack track : animation.iKTracks) {
                if (animTrackBuilder.getIkIndex() != track.ik.index) {
                    builders.add(animTrackBuilder);
                    animTrackBuilder = IKAnimationTrack.newBuilder();
                    animTrackBuilder.setIkIndex(track.ik.index);
                }
                toDDF(track, animTrackBuilder, animation.duration, sampleRate, spf);
            }
            builders.add(animTrackBuilder);
            // Compiled ik tracks must be in ik index order
            Collections.sort(builders, new Comparator<IKAnimationTrack.Builder>() {
                @Override
                public int compare(IKAnimationTrack.Builder o1, IKAnimationTrack.Builder o2) {
                    return o1.getIkIndex() - o2.getIkIndex();
                }
            });
            for (IKAnimationTrack.Builder builder : builders) {
                animBuilder.addIkTracks(builder);
            }
        }

        if (!animation.slotTracks.isEmpty()) {
            for (Map.Entry<Long, Map<String, List<MeshIndex>>> skinEntry : slotIndices.entrySet()) {
                long skinId = skinEntry.getKey();
                Map<String, List<MeshIndex>> slotToMeshIndex = skinEntry.getValue();
                for (RigUtil.SlotAnimationTrack track : animation.slotTracks) {
                    List<MeshIndex> meshIndices = slotToMeshIndex.get(track.slot.name);
                    RigUtil.Slot slot = scene.getSlot(track.slot.name);
                    if (meshIndices != null) {
                        for (MeshIndex meshIndex : meshIndices) {
                            MeshAnimationTrack.Builder trackBuilder = MeshAnimationTrack.newBuilder();
                            trackBuilder.setMeshId(skinId);
                            trackBuilder.setMeshIndex(meshIndex.index);
                            toDDF(slot, track, trackBuilder, animation.duration, sampleRate, spf, meshIndex.name);
                            animBuilder.addMeshTracks(trackBuilder.build());
                        }
                    }
                }
            }
        }
        for (RigUtil.EventTrack track : animation.eventTracks) {
            EventTrack.Builder builder = EventTrack.newBuilder();
            toDDF(track, builder);
            animBuilder.addEventTracks(builder);
        }

        animSetBuilder.addAnimations(animBuilder);
    }

    private static class MeshIndex {
        public MeshIndex(int index, String name) {
            this.index = index;
            this.name = name;
        }

        public int index;
        public String name;
    }

    @Override
    public void build(Task<Void> task) throws CompileExceptionError,
            IOException {

        SpineSceneDesc.Builder builder = SpineSceneDesc.newBuilder();
        ProtoUtil.merge(task.input(0), builder);

        // Load previously created atlas textureset
        TextureSet.Builder resultBuilder = TextureSet.newBuilder();
        resultBuilder.mergeFrom(task.input(2).getContent());

        TextureSet result = resultBuilder.build();
        FloatBuffer uv = ByteBuffer.wrap(result.getTexCoords().toByteArray()).order(ByteOrder.LITTLE_ENDIAN).asFloatBuffer();

        final Map<String, UVTransform> animToTransform = new HashMap<String, UVTransform>();
        for (TextureSetAnimation animation : result.getAnimationsList()) {
            int i = animation.getStart() * 8;

            // We base the rotation on what order minU and maxV comes, see texture_set_ddf.proto
            // If it's rotated the value of minU should be at position 0 and 6.
            animToTransform.put(animation.getId(),
                new UVTransform(new Point2d(uv.get(i), uv.get(i+3)),
                                new Vector2d(uv.get(i+4) - uv.get(i), uv.get(i+7) - uv.get(i+3)),
                                uv.get(i) == uv.get(i+6)) );
        }

        Rig.RigScene.Builder b = Rig.RigScene.newBuilder();
        try {
            SpineSceneUtil scene = SpineSceneUtil.loadJson(new ByteArrayInputStream(task.input(1).getContent()), new UVTransformProvider() {
                @Override
                public UVTransform getUVTransform(String animId) {
                    return animToTransform.get(animId);
                }
            });
            ByteArrayOutputStream out;

            // Skeleton
            Skeleton.Builder skeletonBuilder = Skeleton.newBuilder();
            List<Integer> boneIndexRemap = toDDF(scene.bones, scene.iks, skeletonBuilder);
            int maxBoneCount = skeletonBuilder.getBonesCount();
            out = new ByteArrayOutputStream(64 * 1024);
            skeletonBuilder.setLocalBoneScaling(scene.localBoneScaling);
            skeletonBuilder.build().writeTo(out);
            out.close();
            task.output(1).setContent(out.toByteArray());

            // MeshSet
            MeshSet.Builder meshSetBuilder = MeshSet.newBuilder();
            Map<Long, Map<String, List<MeshIndex>>> slotIndices = new HashMap<Long, Map<String, List<MeshIndex>>>();
            slotIndices.put(MurmurHash.hash64(""), toDDF("", scene.meshes, Collections.<RigUtil.Mesh>emptyList(), meshSetBuilder, boneIndexRemap));
            for (Map.Entry<String, List<RigUtil.Mesh>> entry : scene.skins.entrySet()) {
                slotIndices.put(MurmurHash.hash64(entry.getKey()), toDDF(entry.getKey(), scene.meshes, entry.getValue(), meshSetBuilder, boneIndexRemap));
            }
            meshSetBuilder.setMaxBoneCount(maxBoneCount);
            meshSetBuilder.setSlotCount(scene.getSlotCount());
            out = new ByteArrayOutputStream(64 * 1024);
            meshSetBuilder.build().writeTo(out);
            out.close();
            task.output(2).setContent(out.toByteArray());

            // AnimationSet
            AnimationSet.Builder animSetBuilder = AnimationSet.newBuilder();
            for (Map.Entry<String, RigUtil.Animation> entry : scene.animations.entrySet()) {
                toDDF(scene, entry.getKey(), entry.getValue(), animSetBuilder, builder.getSampleRate(), slotIndices);
            }
            out = new ByteArrayOutputStream(64 * 1024);
            animSetBuilder.build().writeTo(out);
            out.close();
            task.output(3).setContent(out.toByteArray());

        } catch (LoadException e) {
            throw new CompileExceptionError(task.input(1), -1, e.getMessage());
        }

        int buildDirLen = project.getBuildDirectory().length();

        b.setSkeleton(task.output(1).getPath().substring(buildDirLen));
        b.setMeshSet(task.output(2).getPath().substring(buildDirLen));
        b.setAnimationSet(task.output(3).getPath().substring(buildDirLen));
        b.setTextureSet(BuilderUtil.replaceExt(builder.getAtlas(), "atlas", "texturesetc"));

        ArrayList<String> textures = new ArrayList<String>( builder.getTexturesList() );
        if(textures.isEmpty()) {
            b.addTextures(ProtoBuilders.resolveTextureFilename(builder.getAtlas()));
        } else {
            for (String t : textures) {
                b.addTextures(ProtoBuilders.resolveTextureFilename(t));
            }
        }

        Message msg = b.build();
        ByteArrayOutputStream out = new ByteArrayOutputStream(64 * 1024);
        msg.writeTo(out);
        out.close();
        task.output(0).setContent(out.toByteArray());
    }
}

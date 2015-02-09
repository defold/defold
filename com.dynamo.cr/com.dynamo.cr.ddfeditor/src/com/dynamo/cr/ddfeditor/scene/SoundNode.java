package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.sound.proto.Sound.SoundDesc;
import com.google.protobuf.Message;

@SuppressWarnings("serial")
public class SoundNode extends ComponentTypeNode {

    @Property(editorType=EditorType.RESOURCE, extensions={"ogg", "wav"})
    @Resource
    @NotEmpty
    private String sound = "";

    @Property
    private boolean looping = false;

    @Property(displayName = "Gain (db)")
    @Range(min = -100, max = 0)
    private float gain = 1.0f;

    @Property
    private String group = "master";

    public SoundNode(SoundDesc soundDesc) {
        setSound(soundDesc.getSound());
        setLooping(soundDesc.getLooping() != 0);
        setGroup(soundDesc.getGroup());
        setGain(toDB(soundDesc.getGain()));
    }

    static float toDB(float gain) {
        return (float) (20.0f * Math.log10(gain));
    }

    static float fromDB(float db) {
        return (float) Math.exp(db * Math.log(10.0) / 20.0);
    }

    public void setSound(String sound) {
        this.sound = sound;
    }

    public String getSound() {
        return sound;
    }

    public void setLooping(boolean looping) {
        this.looping = looping;
    }

    public boolean isLooping() {
        return looping;
    }

    public float getGain() {
        return gain;
    }

    public void setGain(float gain) {
        this.gain = gain;
    }

    public String getGroup() {
        return group;
    }

    public void setGroup(String group) {
        this.group = group;
    }

    public Message buildMessage() {
        return SoundDesc.newBuilder()
            .setSound(getSound())
            .setLooping(isLooping() ? 1 : 0)
            .setGain(fromDB(gain))
            .setGroup(group)
            .build();
    }

}

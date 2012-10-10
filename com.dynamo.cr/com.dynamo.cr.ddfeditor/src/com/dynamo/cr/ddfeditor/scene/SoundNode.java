package com.dynamo.cr.ddfeditor.scene;

import com.dynamo.cr.go.core.ComponentTypeNode;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
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

    public SoundNode(SoundDesc soundDesc) {
        setSound(soundDesc.getSound());
        setLooping(soundDesc.getLooping() != 0);
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

    public Message buildMessage() {
        return SoundDesc.newBuilder()
            .setSound(getSound())
            .setLooping(isLooping() ? 1 : 0)
            .build();
    }

}

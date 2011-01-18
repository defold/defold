package com.dynamo.cr.ddfeditor;

import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.input.proto.Input.GamepadMap;
import com.dynamo.input.proto.Input.GamepadMapEntry;
import com.dynamo.input.proto.Input.GamepadModifier_t;
import com.google.protobuf.Descriptors.Descriptor;

public class GamepadMapEditor extends DdfEditor {

    public GamepadMapEditor() {
        super("gamepads");
    }

    @Override
    public String getMessageNodeLabelValue(MessageNode messageNode) {
        Descriptor descriptor = messageNode.getDescriptor();
        String name = descriptor.getName();
        if (name.equals("GamepadMap")) {
            GamepadMap gamepadMap =  (GamepadMap) messageNode.build();
            return String.format("%s (%s)", gamepadMap.getDevice(), gamepadMap.getPlatform());
        }
        else if (name.equals("GamepadMapEntry")) {
            GamepadMapEntry gamepadMapEntry =  (GamepadMapEntry) messageNode.build();
            return gamepadMapEntry.getInput().name();
        }
        else if (name.equals("GamepadModifier_t")) {
            GamepadModifier_t gamepadModifier =  (GamepadModifier_t) messageNode.build();
            return gamepadModifier.getMod().name();
        }

        return "";
    }

}


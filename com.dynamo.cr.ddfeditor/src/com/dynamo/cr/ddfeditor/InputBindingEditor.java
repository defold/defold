package com.dynamo.cr.ddfeditor;

import com.dynamo.cr.protobind.MessageNode;
import com.dynamo.input.proto.Input.GamepadTrigger;
import com.dynamo.input.proto.Input.InputBinding;
import com.dynamo.input.proto.Input.KeyTrigger;
import com.dynamo.input.proto.Input.MouseTrigger;
import com.google.protobuf.Descriptors.Descriptor;

public class InputBindingEditor extends DdfEditor {

    public InputBindingEditor() {
        super(InputBinding.newBuilder().buildPartial());
    }

    @Override
    public String getMessageNodeLabelValue(MessageNode messageNode) {
        Descriptor descriptor = messageNode.getDescriptor();
        String name = descriptor.getName();
        if (name.equals("GamepadTrigger")) {
            GamepadTrigger gamepadTrigger =  (GamepadTrigger) messageNode.build();
            return gamepadTrigger.getAction();
        }
        else if (name.equals("KeyTrigger")) {
            KeyTrigger keyTrigger =  (KeyTrigger) messageNode.build();
            return keyTrigger.getAction();
        }
        else if (name.equals("MouseTrigger")) {
            MouseTrigger mouseTrigger =  (MouseTrigger) messageNode.build();
            return mouseTrigger.getAction();
        }
        return "";
    }
}

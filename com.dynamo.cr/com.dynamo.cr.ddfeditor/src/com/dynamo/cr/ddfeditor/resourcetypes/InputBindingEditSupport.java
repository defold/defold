package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.input.proto.Input.Gamepad;
import com.dynamo.input.proto.Input.GamepadTrigger;
import com.dynamo.input.proto.Input.Key;
import com.dynamo.input.proto.Input.KeyTrigger;
import com.dynamo.input.proto.Input.Mouse;
import com.dynamo.input.proto.Input.MouseTrigger;
import com.dynamo.input.proto.Input.Touch;
import com.dynamo.input.proto.Input.TouchTrigger;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class InputBindingEditSupport implements IResourceTypeEditSupport {

    public InputBindingEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(GamepadTrigger.getDescriptor().getFullName())) {
            return GamepadTrigger.newBuilder().setAction("unnamed").setInput(Gamepad.GAMEPAD_RPAD_DOWN).build();
        }
        else if (descriptor.getFullName().equals(KeyTrigger.getDescriptor().getFullName())) {
            return KeyTrigger.newBuilder().setAction("unnamed").setInput(Key.KEY_ENTER).build();
        }
        else if (descriptor.getFullName().equals(MouseTrigger.getDescriptor().getFullName())) {
            return MouseTrigger.newBuilder().setAction("unnamed").setInput(Mouse.MOUSE_BUTTON_1).build();
        }
        else if (descriptor.getFullName().equals(TouchTrigger.getDescriptor().getFullName())) {
            return TouchTrigger.newBuilder().setAction("unnamed").setInput(Touch.TOUCH_MULTI).build();
        }
        return null;
    }

    @Override
    public String getLabelText(Message message) {
        if (message instanceof GamepadTrigger) {
            GamepadTrigger gamepadTrigger =  (GamepadTrigger) message;
            return gamepadTrigger.getAction();
        }
        else if (message instanceof KeyTrigger) {
            KeyTrigger keyTrigger =  (KeyTrigger) message;
            return keyTrigger.getAction();
        }
        else if (message instanceof MouseTrigger) {
            MouseTrigger mouseTrigger =  (MouseTrigger) message;
            return mouseTrigger.getAction();
        }
        else if (message instanceof TouchTrigger) {
            TouchTrigger touchTrigger =  (TouchTrigger) message;
            return touchTrigger.getAction();
        }

        return "";
    }
}

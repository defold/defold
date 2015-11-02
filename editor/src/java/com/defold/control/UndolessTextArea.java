package com.defold.control;

import java.util.Optional;

import javafx.event.EventHandler;
import javafx.scene.input.KeyEvent;
import javafx.scene.control.TextArea;

import com.sun.javafx.scene.control.behavior.KeyBinding;
import com.sun.javafx.scene.control.behavior.TextAreaBehavior;

public class UndolessTextArea extends TextArea {

    public UndolessTextArea() {
	super();
	undoBinding = BindingAccessor.findBinding("Undo");
	redoBinding = BindingAccessor.findBinding("Redo");
	addEventHandler(KeyEvent.ANY, keyEventListener);
    }

    static class BindingAccessor extends TextAreaBehavior {
	BindingAccessor(final TextArea textArea) {
	    super(textArea);
	}
	static KeyBinding findBinding(String action) {
	    return TEXT_AREA_BINDINGS.stream().filter(b -> b.getAction() == action).findAny().orElse(null);
	}
    }
    
    private final EventHandler<KeyEvent> keyEventListener =
	new EventHandler<KeyEvent>() {
	    @Override public void handle(KeyEvent event) {
		if (!event.isConsumed()) {
		    if (matchBinding(undoBinding, event) || matchBinding(redoBinding, event)) {
			event.consume();
		    }
		}
	    }
	};

    private static boolean matchBinding(KeyBinding binding, KeyEvent keyEvent) {
	return binding != null && binding.getCode() == keyEvent.getCode();
    }
	
    private KeyBinding undoBinding;
    private KeyBinding redoBinding;
}

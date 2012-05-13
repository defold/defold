package com.dynamo.cr.tileeditor.commands;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.handlers.RadioState;

import com.dynamo.cr.tileeditor.TileSetEditor;

public class SetBrushCollisionGroup extends AbstractHandler {

    public static final String COMMAND_ID = "com.dynamo.cr.tileeditor.commands.SetBrushCollisionGroup";

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart activeEditor = HandlerUtil.getActiveEditor(event);
        if (activeEditor instanceof TileSetEditor) {
            if(HandlerUtil.matchesRadioState(event))
                return null; // we are already in the updated state - do nothing

            TileSetEditor editor = (TileSetEditor)activeEditor;
            String currentState = event.getParameter(RadioState.PARAMETER_ID);
            int index = Integer.parseInt(currentState);
            editor.setBrushCollisionGroup(index);

            // and finally update the current state
            HandlerUtil.updateRadioState(event.getCommand(), currentState);
        }
        return null;
    }

}

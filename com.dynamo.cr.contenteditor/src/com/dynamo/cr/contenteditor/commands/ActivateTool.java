package com.dynamo.cr.contenteditor.commands;

import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.commands.ICommandService;
import org.eclipse.ui.commands.IElementUpdater;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.handlers.RadioState;
import org.eclipse.ui.menus.UIElement;

import com.dynamo.cr.contenteditor.editors.IEditor;

public class ActivateTool extends AbstractHandler implements IElementUpdater {
    public static final String ACTIVATE_TOOL_COMMAND_ID = "com.dynamo.cr.contenteditor.commands.activateTool";

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IEditorPart editor = HandlerUtil.getActiveEditor(event);
        if (editor instanceof IEditor) {
            if(HandlerUtil.matchesRadioState(event))
                return null; // we are already in the updated state - do nothing

            String currentState = event.getParameter(RadioState.PARAMETER_ID);

            // do whatever having "currentState" implies
            ((IEditor) editor).setManipulator(currentState);

            // and finally update the current state
            HandlerUtil.updateRadioState(event.getCommand(), currentState);

        }
        return null;
    }

    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {
        // This is a workaround for a bug in GTK
        // If the element is disabled, it still incorrectly reports its status to be checked through:
        // OS.gtk_toggle_button_get_active
        // which stops it from being rendered as checked when enabled.
        Object radioStateParameter = parameters.get(RadioState.PARAMETER_ID);
        if (radioStateParameter != null) {
            ICommandService service = (ICommandService) element.getServiceLocator().getService(ICommandService.class);
            if (service.getCommand(ACTIVATE_TOOL_COMMAND_ID).getState(RadioState.STATE_ID).getValue().equals(radioStateParameter)) {
                element.setChecked(false);
                element.setChecked(true);
            }
        }
    }
}

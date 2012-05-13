package com.dynamo.cr.sceneed.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.handlers.RadioState;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.IManipulatorMode;
import com.dynamo.cr.sceneed.core.IManipulatorRegistry;
import com.dynamo.cr.sceneed.core.ManipulatorController;
import com.dynamo.cr.sceneed.ui.SceneEditor;

public class SelectManipulatorModeHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        SceneEditor editor = (SceneEditor) HandlerUtil.getActiveEditor(event);
        ManipulatorController manipulatorController = editor.getManipulatorController();
        IManipulatorRegistry manipulatorRegistry = Activator.getDefault().getManipulatorRegistry();
        String stringMode = event.getParameter(RadioState.PARAMETER_ID);
        IManipulatorMode mode = manipulatorRegistry.getMode(stringMode);
        manipulatorController.setManipulatorMode(mode);
        HandlerUtil.updateRadioState(event.getCommand(), stringMode);
        return null;
    }

}

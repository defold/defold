package com.dynamo.cr.contenteditor.commands;

import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.swt.SWT;
import org.eclipse.ui.commands.ICommandService;
import org.eclipse.ui.commands.IElementUpdater;
import org.eclipse.ui.handlers.RadioState;
import org.eclipse.ui.menus.UIElement;

public abstract class AbstractRadioHandler extends AbstractHandler implements IElementUpdater {

    abstract public String getCommandId();

    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {

        // This is a workaround for a bug in GTK
        // If the element is disabled, it still incorrectly reports its status to be checked through:
        // OS.gtk_toggle_button_get_active
        // which stops it from being rendered as checked when enabled.
        if (SWT.getPlatform().equals("gtk")) {
            Object radioStateParameter = parameters.get(RadioState.PARAMETER_ID);
            if (radioStateParameter != null) {
                ICommandService service = (ICommandService) element.getServiceLocator().getService(ICommandService.class);
                if (service.getCommand(getCommandId()).getState(RadioState.STATE_ID).getValue().equals(radioStateParameter)) {
                    element.setChecked(false);
                    element.setChecked(true);
                }
            }
        }

    }

}

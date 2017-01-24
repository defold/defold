package com.dynamo.cr.editor.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.ui.handlers.HandlerUtil;

import com.dynamo.cr.editor.liveupdate.LiveUpdateDialog;
import com.dynamo.cr.editor.liveupdate.LiveUpdatePresenter;

public class LiveUpdateHandler extends AbstractHandler {

	@Override
	public Object execute(ExecutionEvent event) throws ExecutionException {
        Shell parentShell = HandlerUtil.getActiveShell(event);
		LiveUpdateDialog dialog = new LiveUpdateDialog(parentShell);
		LiveUpdatePresenter presenter = new LiveUpdatePresenter();
		dialog.setPresenter(presenter);
		dialog.open();
		
		return null;
	}

}

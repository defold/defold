package com.dynamo.cr.parted.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.IHandler;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.part.IPage;
import org.eclipse.ui.part.PageBookView;

import com.dynamo.cr.parted.curve.ICurveView;

public class FrameCurves extends AbstractHandler implements IHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IWorkbenchPart part = HandlerUtil.getActivePart(event);
        if (part instanceof PageBookView) {
            IPage page = ((PageBookView)part).getCurrentPage();
            if (page instanceof ICurveView) {
                ICurveView view = (ICurveView)page;
                view.frame();
            }
        }
        return null;
    }

}

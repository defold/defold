package com.dynamo.cr.parted.handlers;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.part.IPage;
import org.eclipse.ui.part.PageBookView;

import com.dynamo.cr.parted.curve.CurveViewer;
import com.dynamo.cr.parted.views.ParticleFXCurveEditorPage;

public class AddPointHandler extends AbstractHandler {

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IWorkbenchPart part = HandlerUtil.getActivePart(event);
        Display display = Display.getCurrent();
        if (part instanceof PageBookView) {
            IPage page = ((PageBookView)part).getCurrentPage();
            if (page instanceof ParticleFXCurveEditorPage) {
                ParticleFXCurveEditorPage view = (ParticleFXCurveEditorPage)page;
                CurveViewer viewer = view.getCurveViewer();
                viewer.addPoint(display.map(null, view.getCurveViewer(), display.getCursorLocation()));
            }
        }
        return null;
    }

}

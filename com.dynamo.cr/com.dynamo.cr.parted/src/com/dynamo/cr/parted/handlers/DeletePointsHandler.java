package com.dynamo.cr.parted.handlers;

import java.util.Map;

import org.eclipse.core.commands.AbstractHandler;
import org.eclipse.core.commands.ExecutionEvent;
import org.eclipse.core.commands.ExecutionException;
import org.eclipse.ui.IViewSite;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.commands.IElementUpdater;
import org.eclipse.ui.handlers.HandlerUtil;
import org.eclipse.ui.menus.UIElement;
import org.eclipse.ui.part.IPage;
import org.eclipse.ui.part.PageBookView;
import org.eclipse.ui.services.IServiceScopes;

import com.dynamo.cr.parted.curve.CurvePresenter;
import com.dynamo.cr.parted.views.CurveEditorView;
import com.dynamo.cr.parted.views.ParticleFXCurveEditorPage;

public class DeletePointsHandler extends AbstractHandler implements IElementUpdater {

    public static final String COMMAND_ID = "org.eclipse.ui.edit.delete";

    @Override
    public Object execute(ExecutionEvent event) throws ExecutionException {
        IWorkbenchPart part = HandlerUtil.getActivePart(event);
        if (part instanceof PageBookView) {
            IPage page = ((PageBookView)part).getCurrentPage();
            if (page instanceof ParticleFXCurveEditorPage) {
                ParticleFXCurveEditorPage view = (ParticleFXCurveEditorPage)page;
                view.getPresenter().onRemove();
            }
        }
        return null;
    }

    @Override
    public void updateElement(UIElement element, @SuppressWarnings("rawtypes") Map parameters) {
        Object partSite = parameters.get(IServiceScopes.PARTSITE_SCOPE);
        if (partSite != null) {
            IViewSite site = (IViewSite)partSite;
            IWorkbenchPart part = site.getPart();
            if (part instanceof CurveEditorView) {
                ParticleFXCurveEditorPage page = (ParticleFXCurveEditorPage)((CurveEditorView)part).getCurrentPage();
                CurvePresenter presenter = page.getPresenter();
                setBaseEnabled(presenter.hasRemovablePoints());
            }
        }
    }
}

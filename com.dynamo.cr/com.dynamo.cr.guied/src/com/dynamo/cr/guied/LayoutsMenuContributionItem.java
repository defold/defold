package com.dynamo.cr.guied;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.jface.action.IContributionItem;
import org.eclipse.swt.SWT;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.CompoundContributionItem;
import org.eclipse.ui.menus.CommandContributionItem;
import org.eclipse.ui.menus.CommandContributionItemParameter;
import org.eclipse.ui.services.IServiceLocator;

import com.dynamo.cr.guied.core.GuiSceneNode;
import com.dynamo.cr.guied.util.Layouts;
import com.dynamo.cr.guied.GuiSceneEditor;

public class LayoutsMenuContributionItem extends CompoundContributionItem {

    private IServiceLocator serviceLocator;

    public LayoutsMenuContributionItem(IServiceLocator serviceLocator) {
        this.serviceLocator = serviceLocator;
    }

    @Override
    public boolean isDynamic() {
        return true;
    }

    @Override
    protected IContributionItem[] getContributionItems() {
        final IEditorPart editorPart = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor();
        if (!(editorPart instanceof GuiSceneEditor)) {
            return null;
        }

        GuiSceneNode scene  = (GuiSceneNode)((GuiSceneEditor)editorPart).getModel().getRoot();
        assert((scene instanceof GuiSceneNode) == true);
        List<Layouts.Layout> layoutList = Layouts.getLayoutList(scene.getLayouts());
        IContributionItem[] list = new IContributionItem[layoutList.size()];
        int i = 0;
        for (Layouts.Layout layout : layoutList) {
            CommandContributionItemParameter param = new CommandContributionItemParameter(
                    serviceLocator, null, "com.dynamo.cr.guied.commands.selectLayout", SWT.RADIO);
            String itemId = layout.getId();
            param.label = itemId;
            Map<String, String> parameters = new HashMap<String, String>();
            parameters.put("org.eclipse.ui.commands.radioStateParameter", itemId);
            param.parameters = parameters;
            list[i] = new CommandContributionItem(param);
            ++i;
        }
        return list;
    }

}

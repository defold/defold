package com.dynamo.cr.tileeditor;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.jface.action.IContributionItem;
import org.eclipse.ui.IEditorPart;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.CompoundContributionItem;
import org.eclipse.ui.handlers.RadioState;
import org.eclipse.ui.menus.CommandContributionItem;
import org.eclipse.ui.menus.CommandContributionItemParameter;

public class CollisionGroupsContributionItem extends
CompoundContributionItem {

    private final static String COMMAND_ID = "com.dynamo.cr.tileeditor.commands.SetBrushCollisionGroup";

    public CollisionGroupsContributionItem() {
        // TODO Auto-generated constructor stub
    }

    @Override
    protected IContributionItem[] getContributionItems() {
        if (PlatformUI.getWorkbench() == null
                || PlatformUI.getWorkbench().getActiveWorkbenchWindow() == null) {
            return null;
        }
        final IEditorPart editorPart = PlatformUI.getWorkbench().getActiveWorkbenchWindow().getActivePage().getActiveEditor();
        if (editorPart instanceof TileSetEditor) {
            TileSetEditor editor = (TileSetEditor)editorPart;
            List<String> collisionGroups = new ArrayList<String>(editor.getCollisionGroups());
            collisionGroups.add(0, "(none)");
            int n = collisionGroups.size();
            IContributionItem[] items = new IContributionItem[n];
            for (int i = 0; i < n; ++i) {
                String id = "com.dynamo.cr.tileeditor.commands.setBrushCollisionGroup" + n;
                CommandContributionItemParameter param = new CommandContributionItemParameter(PlatformUI.getWorkbench(), id, COMMAND_ID, CommandContributionItem.STYLE_RADIO);
                param.label = collisionGroups.get(i);
                Map<String, String> parameters = new HashMap<String, String>();
                parameters.put(RadioState.PARAMETER_ID, Integer.toString(i-1));
                param.parameters = parameters;
                items[i] = new CommandContributionItem(param);
            }
            return items;
        }
        return null;
    }

}

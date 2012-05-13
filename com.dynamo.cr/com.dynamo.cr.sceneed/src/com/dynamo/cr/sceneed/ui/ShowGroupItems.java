package com.dynamo.cr.sceneed.ui;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;

import org.eclipse.jface.action.IContributionItem;
import org.eclipse.ui.actions.CompoundContributionItem;
import org.eclipse.ui.menus.CommandContributionItem;
import org.eclipse.ui.menus.CommandContributionItemParameter;

import com.dynamo.cr.sceneed.Activator;
import com.dynamo.cr.sceneed.core.INodeType;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.handlers.ShowGroupHandler;

public class ShowGroupItems extends CompoundContributionItem {

    @SuppressWarnings({"unchecked", "rawtypes"})
    @Override
    protected IContributionItem[] getContributionItems() {
        INodeTypeRegistry registry = Activator.getDefault().getNodeTypeRegistry();
        List<String> groups = new ArrayList<String>();
        for (INodeType type : registry.getNodeTypes()) {
            String group = type.getDisplayGroup();
            if (group != null && !group.isEmpty()) {
                if (!groups.contains(type.getDisplayGroup())) {
                    groups.add(type.getDisplayGroup());
                }
            }
        }
        Collections.sort(groups);
        IContributionItem[] items = new IContributionItem[groups.size()];
        int i = 0;
        for (String group : groups) {
            CommandContributionItemParameter param = new CommandContributionItemParameter(Activator.getDefault().getWorkbench(), "displayGroup." + group, ShowGroupHandler.COMMAND_ID, CommandContributionItem.STYLE_CHECK);
            param.label = group;
            param.parameters = new HashMap();
            param.parameters.put(ShowGroupHandler.GROUP_PARAMETER_ID, group);
            items[i] = new CommandContributionItem(param);
            ++i;
        }
        return items;
    }

}

package com.dynamo.cr.targets;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.jface.action.IContributionItem;
import org.eclipse.swt.SWT;
import org.eclipse.ui.actions.CompoundContributionItem;
import org.eclipse.ui.menus.CommandContributionItem;
import org.eclipse.ui.menus.CommandContributionItemParameter;
import org.eclipse.ui.services.IServiceLocator;

import com.dynamo.cr.targets.core.ITarget;
import com.dynamo.cr.targets.core.TargetsPlugin;

public class TargetsContributionItem extends CompoundContributionItem {

    private IServiceLocator serviceLocator;

    public TargetsContributionItem(IServiceLocator serviceLocator) {
        this.serviceLocator = serviceLocator;
    }

    @Override
    public boolean isDynamic() {
        return true;
    }

    @Override
    protected IContributionItem[] getContributionItems() {
        ITarget[] targets = TargetsPlugin.getDefault().getTargets();
        IContributionItem[] list = new IContributionItem[targets.length ];
        int i = 0;
        for (ITarget target : targets) {
            CommandContributionItemParameter param = new CommandContributionItemParameter(
                    serviceLocator, null, "com.dynamo.cr.targets.commands.selectTarget", SWT.RADIO);
            param.label = target.getName();
            Map<String, String> parameters = new HashMap<String, String>();
            // NOTE: We are using name instead of id as radio-state parameters
            // in order to keep the local target selected. We started with a pseudo-local target
            // and when the engine starts the actual local target is found. These target have different
            // id's but the same name
            parameters.put("org.eclipse.ui.commands.radioStateParameter", target.getName());
            param.parameters = parameters;
            list[i] = new CommandContributionItem(param);
            ++i;
        }

        /*
        CommandContributionItemParameter param = new CommandContributionItemParameter(
                serviceLocator, null, "com.dynamo.cr.targets.commands.selectTarget", SWT.RADIO);
        param.label = "hej";
        Map<String, String> parameters = new HashMap<String, String>();
        parameters.put("org.eclipse.ui.commands.radioStateParameter", "hej");
        param.parameters = parameters;
        list[i] = new CommandContributionItem(param);
        ++i;*/

        return list;
    }

}

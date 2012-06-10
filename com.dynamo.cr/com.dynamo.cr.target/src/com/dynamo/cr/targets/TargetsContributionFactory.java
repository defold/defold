package com.dynamo.cr.targets;

import org.eclipse.ui.menus.ExtensionContributionFactory;
import org.eclipse.ui.menus.IContributionRoot;
import org.eclipse.ui.services.IServiceLocator;

public class TargetsContributionFactory extends ExtensionContributionFactory {

    public TargetsContributionFactory() {
    }

    @Override
    public void createContributionItems(IServiceLocator serviceLocator,
            IContributionRoot additions) {
        additions.addContributionItem(new TargetsContributionItem(serviceLocator), null);
    }

}

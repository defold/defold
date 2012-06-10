package com.dynamo.cr.target;

import org.eclipse.ui.menus.ExtensionContributionFactory;
import org.eclipse.ui.menus.IContributionRoot;
import org.eclipse.ui.services.IServiceLocator;

public class TargetContributionFactory extends ExtensionContributionFactory {

    public TargetContributionFactory() {
    }

    @Override
    public void createContributionItems(IServiceLocator serviceLocator,
            IContributionRoot additions) {
        additions.addContributionItem(new TargetContributionItem(serviceLocator), null);
    }

}

package com.dynamo.cr.guied;

import org.eclipse.ui.menus.ExtensionContributionFactory;
import org.eclipse.ui.menus.IContributionRoot;
import org.eclipse.ui.services.IServiceLocator;

public class LayoutsMenuContributionFactory extends ExtensionContributionFactory {

    public LayoutsMenuContributionFactory() {
    }

    @Override
    public void createContributionItems(IServiceLocator serviceLocator,
            IContributionRoot additions) {
        additions.addContributionItem(new LayoutsMenuContributionItem(serviceLocator), null);
    }

}

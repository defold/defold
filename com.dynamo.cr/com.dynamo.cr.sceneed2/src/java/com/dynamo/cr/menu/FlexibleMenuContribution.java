package com.dynamo.cr.menu;

import org.eclipse.jface.action.IContributionItem;
import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.actions.CompoundContributionItem;
import org.eclipse.ui.menus.CommandContributionItem;
import org.eclipse.ui.menus.CommandContributionItemParameter;

import clojure.lang.RT;
import clojure.osgi.ClojureHelper;

public class FlexibleMenuContribution extends CompoundContributionItem {
    private boolean requiredNamespace = false;

    private final String extensionPoint;
    private final IWorkbenchWindow window;

    public FlexibleMenuContribution(IWorkbenchWindow window, String extensionPoint) {
        this.window = window;
        this.extensionPoint = extensionPoint;
    }

    @Override
    public boolean isDynamic() {
        return true;
    }

    @Override
    protected IContributionItem[] getContributionItems() {
        ensureNamespace("internal.ui.menus");
        Object items = ClojureHelper.invoke("internal.ui.menus", "collect-items");
        IContributionItem[] result = new IContributionItem[RT.count(items)];

        for (int i = 0; i < RT.count(items); i++) {
            result[i] = asContributionItem(RT.nth(items, i));
        }

        return result;
    }

    private synchronized void ensureNamespace(String ns) {
        if (!requiredNamespace) {
            ClojureHelper.require(ns);
            requiredNamespace = true;
        }
    }

    private IContributionItem asContributionItem(Object clojureData) {
        // clojure data: '(label command-id & [mnemonic image-id
        // disabled-image-id])
        String label = (String) RT.first(clojureData);
        String commandId = (String) RT.second(clojureData);
        String mnemonic = (String) (RT.count(clojureData) > 2 ? RT.nth(clojureData, 2) : null);
        String imageId = (String) (RT.count(clojureData) > 3 ? RT.nth(clojureData, 3) : null);
        String disabledImageId = (String) (RT.count(clojureData) > 4 ? RT.nth(clojureData, 4) : null);
        return makeContributionItem(label, commandId, mnemonic, imageId, disabledImageId);
    }

    private IContributionItem makeContributionItem(String label, String commandId, String mnemonic, String image, String disabledImage) {
        ISharedImages sharedImages = window.getWorkbench().getSharedImages();

        CommandContributionItemParameter commandParm = new CommandContributionItemParameter(window, commandId, commandId, null, sharedImages.getImageDescriptor(image), sharedImages.getImageDescriptor(disabledImage), null, label, mnemonic, null,
                CommandContributionItem.STYLE_PUSH, null, false);
        return new CommandContributionItem(commandParm);
    }
}

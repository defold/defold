package com.dynamo.cr.menu;

import org.eclipse.jface.action.IContributionItem;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.actions.CompoundContributionItem;

import clojure.lang.RT;
import clojure.osgi.ClojureHelper;

public class FlexibleMenuItemContribution extends CompoundContributionItem {
    private boolean requiredNamespace = false;

    private final String extensionPoint;
    private final IWorkbenchWindow window;
    private final String categoryId;

    public FlexibleMenuItemContribution(IWorkbenchWindow window, String extensionPoint, String categoryId) {
        this.window = window;
        this.extensionPoint = extensionPoint;
        this.categoryId = categoryId;
    }

    protected String getExtensionPoint() {
        return this.extensionPoint;
    }

    public static IContributionItem addMenuItems(IWorkbenchWindow window, String extensionPoint, String categoryId) {
        return new FlexibleMenuItemContribution(window, extensionPoint, categoryId);
    }

    @Override
    protected IContributionItem[] getContributionItems() {
        return getCategoryItems(this.categoryId);
    }

    protected IContributionItem[] getCategoryItems(String categoryId) {
        ensureNamespace("internal.ui.menus");
        Object items = ClojureHelper.invoke("internal.ui.menus", "collect-category-items", categoryId);

        int itemCount = RT.count(items);
        FlexibleContributionItem[] results = new FlexibleContributionItem[itemCount];
        for (int i = 0; i < itemCount; i++) {
            results[i] = FlexibleContributionItem.fromClojureData(RT.nth(items, i), this.window);
        }
        return results;
    }

    // clojure helper
    private synchronized void ensureNamespace(String ns) {
        if (!requiredNamespace) {
            ClojureHelper.require(ns);
            requiredNamespace = true;
        }
    }

}

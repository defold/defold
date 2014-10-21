/**
 *
 */
package com.dynamo.cr.menu;

import org.eclipse.ui.ISharedImages;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.menus.CommandContributionItem;
import org.eclipse.ui.menus.CommandContributionItemParameter;

import clojure.lang.RT;

/**
 * This class creates a command contribution item capable of being used as a
 * menu item from Clojure data. It is expected to be called from
 * FlexibleMenuItemContribution and FlexibleMenuContribution, consolidating the
 * creation of command contributions into a single place.
 *
 * @author Ben Vandgrift
 */
public class FlexibleContributionItem extends CommandContributionItem {

    public FlexibleContributionItem(CommandContributionItemParameter contributionParameters) {
        super(contributionParameters);
    }

    public static FlexibleContributionItem fromClojureData(Object clojureData, IWorkbenchWindow window) {
        // expects clojure data: '(label command-id & [mnemonic image-id
        // disabled-image-id])
        String label = (String) RT.first(clojureData);
        String commandId = (String) RT.second(clojureData);
        String mnemonic = (String) (RT.count(clojureData) > 2 ? RT.nth(clojureData, 2) : null);
        String imageId = (String) (RT.count(clojureData) > 3 ? RT.nth(clojureData, 3) : null);
        String disabledImageId = (String) (RT.count(clojureData) > 4 ? RT.nth(clojureData, 4) : null);
        ISharedImages sharedImages = window.getWorkbench().getSharedImages();

        CommandContributionItemParameter commandParm = new CommandContributionItemParameter(window, commandId, commandId, null, sharedImages.getImageDescriptor(imageId), sharedImages.getImageDescriptor(disabledImageId), null, label, mnemonic, null,
                CommandContributionItem.STYLE_PUSH, null, false);
        return new FlexibleContributionItem(commandParm);
    }
}

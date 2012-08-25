package com.dynamo.cr.target.bundle;

import java.io.File;
import java.io.IOException;
import java.util.Collection;
import java.util.EnumMap;
import java.util.Map;

import javax.inject.Inject;

import com.dynamo.cr.target.sign.IIdentityLister;

public class BundleiOSPresenter implements BundleiOSDialog.IPresenter {

    private IBundleiOSView view;
    private IIdentityLister lister;
    private String profile = "";
    private String identity = "";
    private String[] identities = new String[0];
    private Map<IconType, String> icons = new EnumMap<BundleiOSPresenter.IconType, String>(IconType.class);
    private String applicationName = "";
    private String applicationIdentifier = "";
    private String version = "1.0";
    private boolean preRendered;

    public enum IconType
    {
        IPHONE(57, false),
        IPHONE_HIGH(57 * 2, true),
        IPAD(72, false),
        IPAD_HIGH(72 * 2, true);

        private int size;
        private boolean high;

        IconType(int size, boolean high)
        {
            this.size = size;
            this.high = high;
        }

        public int getSize() {
            return size;
        }

        public boolean isHigh() {
            return high;
        }
    }

    @Inject
    public BundleiOSPresenter(IBundleiOSView view, IIdentityLister lister) {
        this.view = view;
        this.lister = lister;
    }

    @Override
    public void start() {
        try {
            identities = lister.listIdentities();
            view.setIdentities(identities);
            view.setVersion(version);
            validate();
        } catch (IOException e) {
            view.setErrorMessage(e.getMessage());
        }
    }

    private void validate() {
        // Set defaults
        view.setEnabled(false);
        view.setMessage(Messages.BundleiOSPresenter_DIALOG_MESSAGE);

        if (identities.length == 0) {
            view.setErrorMessage(Messages.BundleiOSPresenter_NO_IDENTITY_FOUND);
            return;
        }

        if (identity.equals("") || profile.equals("") || applicationName.equals("") || applicationIdentifier.equals("") || version.equals("")) {
            // All values not set yet. Just return
            return;
        }

        if (!new File(profile).isFile()) {
            view.setErrorMessage(Messages.BundleiOSPresenter_PROFILE_NOT_FOUND);
            return;
        }

        // Only warnings after this point
        view.setEnabled(true);

        if (this.icons.size() == 0) {
            view.setWarningMessage(Messages.BundleiOSPresenter_NO_ICONS_SPECIFIED);
            return;
        }

        if (this.icons.size() < 4) {
            view.setWarningMessage("Missing icons");
            return;
        }

    }

    @Override
    public void setIdentity(String identity) {
        this.identity = identity;
        validate();
    }

    public String getIdentity() {
        return identity;
    }

    @Override
    public void setProvisioningProfile(String profile) {
        this.profile = profile;
        validate();
    }

    public String getProvisioningProfile() {
        return profile;
    }

    @Override
    public void setApplicationName(String applicationName) {
        this.applicationName = applicationName;
        validate();
    }

    public String getApplicationName() {
        return applicationName;
    }

    @Override
    public void setApplicationIdentifier(String applicationIdentifier) {
        this.applicationIdentifier = applicationIdentifier;
        validate();
    }

    public String getApplicationIdentifier() {
        return applicationIdentifier;
    }

    @Override
    public void setIcon(String fileName, IconType iconType) {
        icons.put(iconType, fileName);
        validate();
    }

    public Collection<String> getIcons() {
        return icons.values();
    }

    @Override
    public void setPrerendered(boolean preRendered) {
        this.preRendered = preRendered;
        validate();
    }

    public boolean isPreRendered() {
        return preRendered;
    }

    @Override
    public void setVersion(String version) {
        this.version = version;
        validate();
    }


}

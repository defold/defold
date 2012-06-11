package com.dynamo.cr.target.sign;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Iterator;

import javax.inject.Inject;

import org.apache.commons.configuration.ConfigurationException;
import org.apache.commons.configuration.plist.XMLPropertyListConfiguration;
import org.apache.commons.io.IOUtils;

import com.google.common.io.Files;

public class SignPresenter implements SignDialog.IPresenter {

    private ISignView view;
    private IIdentityLister lister;
    private String profile;
    private String identity;
    private String[] identities = new String[0];

    @Inject
    public SignPresenter(ISignView view, IIdentityLister lister) {
        this.view = view;
        this.lister = lister;
    }

    public String getIdentity() {
        return identity;
    }

    public String getProfile() {
        return profile;
    }

    @Override
    public void signAndUpload() {

    }

    @Override
    public void start() {
        view.setMessage(Messages.SignDialog_DIALOG_MESSAGE);
        try {
            identities = lister.listIdentities();
            view.setIdentities(identities);
            validate();
        } catch (IOException e) {
            view.setErrorMessage(e.getMessage());
        }
    }

    private void validate() {
        // Set defaults
        view.setEnabled(false);
        view.setMessage(Messages.SignDialog_DIALOG_MESSAGE);

        if (identities.length == 0) {
            view.setErrorMessage(Messages.SignPresenter_NO_IDENTITY_FOUND);
            return;
        }

        if (identity == null || profile == null) {
            // All values not set yet. Just return
            return;
        }

        if (!new File(profile).isFile()) {
            view.setErrorMessage(Messages.SignPresenter_PROFILE_NOT_FOUND);
            return;
        }

        view.setEnabled(true);
    }

    @Override
    public void setProvisioningProfile(String profile) {
        this.profile = profile;
        validate();
    }

    @Override
    public void setIdentity(String identity) {
        this.identity = identity;
        validate();
    }

}

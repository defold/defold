package com.dynamo.cr.target.core.test;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.File;
import java.util.HashMap;
import java.util.Map;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;

import com.dynamo.cr.target.sign.IIdentityLister;
import com.dynamo.cr.target.sign.ISignView;
import com.dynamo.cr.target.sign.Messages;
import com.dynamo.cr.target.sign.SignPresenter;
import com.dynamo.cr.target.sign.Signer;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class SignTest {

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ISignView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    private ISignView view;
    private IIdentityLister identityLister;
    private Injector injector;
    private SignPresenter presenter;

    @Before
    public void setUp() throws Exception {
        view = mock(ISignView.class);
        identityLister = mock(IIdentityLister.class);
        Module module = new Module();
        injector = Guice.createInjector(module);
        presenter = injector.getInstance(SignPresenter.class);
    }

    @Test
    public void testNoIdentity() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[0]);
        presenter.start();
        verify(view, times(1)).setErrorMessage(Messages.SignPresenter_NO_IDENTITY_FOUND);
        verify(view, times(1)).setEnabled(false);
        verify(view, times(0)).setEnabled(true);
    }

    @Test
    public void testProfileNotFound() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[] { "ident" });
        presenter.start();
        presenter.setProvisioningProfile("does not exists");
        presenter.setIdentity("foo");
        verify(view, times(1)).setErrorMessage(Messages.SignPresenter_PROFILE_NOT_FOUND);
        verify(view, times(3)).setEnabled(false);
        verify(view, times(0)).setEnabled(true);
    }

    @Test
    public void testOk() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[] { "ident" });
        presenter.start();
        File profile = File.createTempFile("test", ".mobileprovision");
        profile.deleteOnExit();
        presenter.setProvisioningProfile(profile.getAbsolutePath());
        presenter.setIdentity("foo");
        verify(view, times(3)).setEnabled(false);
        verify(view, times(1)).setEnabled(true);
    }

    @Test
    @Ignore
    public void testSign() throws Exception {
        /*
         * NOTE: This test is disabled by default as it requires certificates in the local key-chain
         * The test is however good to have when developing/testing stuff
         */

        Signer signer = new Signer();
        Map<String, String> properties = new HashMap<String, String>();
        properties.put("CFBundleDisplayName", "Testing");
        properties.put("CFBundleExecutable", "test_exe");
        String ipaPath = signer.sign("iPhone Developer: Christian MURRAY (QXZXCL5J5G)", "../../share/engine_profile.mobileprovision", "test/test_exe", properties);

        System.out.println(ipaPath);
    }

}

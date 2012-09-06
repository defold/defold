package com.dynamo.cr.target.bundle;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.File;

import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.target.sign.IIdentityLister;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class BundleiOSTest {

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(IBundleiOSView.class).toInstance(view);
            bind(IIdentityLister.class).toInstance(identityLister);
        }
    }

    private IBundleiOSView view;
    private IIdentityLister identityLister;
    private Injector injector;
    private BundleiOSPresenter presenter;

    @Before
    public void setUp() throws Exception {
        view = mock(IBundleiOSView.class);
        identityLister = mock(IIdentityLister.class);
        Module module = new Module();
        injector = Guice.createInjector(module);
        presenter = injector.getInstance(BundleiOSPresenter.class);
    }

    @Test
    public void testNoIdentity() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[0]);
        presenter.start();
        verify(view, times(1)).setErrorMessage(Messages.BundleiOSPresenter_NO_IDENTITY_FOUND);
        verify(view, times(1)).setEnabled(false);
        verify(view, times(0)).setEnabled(true);
    }

    @Test
    public void testProfileNotFound() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[] { "ident" });
        presenter.start();
        presenter.setProvisioningProfile("does not exists");
        presenter.setIdentity("foo");
        verify(view, times(1)).setErrorMessage(Messages.BundleiOSPresenter_PROFILE_NOT_FOUND);
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

}

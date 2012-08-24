package com.dynamo.cr.target.core.test;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.junit.Before;
import org.junit.Ignore;
import org.junit.Test;

import com.dynamo.cr.target.bundle.BundleiOSPresenter;
import com.dynamo.cr.target.bundle.BundleiOSPresenter.IconType;
import com.dynamo.cr.target.bundle.IBundleiOSView;
import com.dynamo.cr.target.bundle.IOSBundler;
import com.dynamo.cr.target.bundle.Messages;
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
        presenter.setApplicationName("My App");
        presenter.setApplicationIdentifier("com.myapp");
        verify(view, times(1)).setErrorMessage(Messages.BundleiOSPresenter_PROFILE_NOT_FOUND);
        verify(view, times(5)).setEnabled(false);
        verify(view, times(0)).setEnabled(true);
    }

    @Test
    public void testMissingIcons1() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[] { "ident" });
        presenter.start();
        File profile = File.createTempFile("test", ".mobileprovision");
        profile.deleteOnExit();
        presenter.setProvisioningProfile(profile.getAbsolutePath());
        presenter.setIdentity("foo");
        presenter.setApplicationName("My App");
        presenter.setApplicationIdentifier("com.myapp");
        verify(view, times(1)).setWarningMessage(Messages.BundleiOSPresenter_NO_ICONS_SPECIFIED);
        verify(view, times(5)).setEnabled(false);
        verify(view, times(1)).setEnabled(true);
    }

    @Test
    public void testMissingIcons2() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[] { "ident" });
        presenter.start();
        File profile = File.createTempFile("test", ".mobileprovision");
        profile.deleteOnExit();
        presenter.setProvisioningProfile(profile.getAbsolutePath());
        presenter.setIdentity("foo");
        presenter.setApplicationName("My App");
        presenter.setApplicationIdentifier("com.myapp");
        presenter.setIcon("test.png", IconType.IPHONE);
        verify(view, times(1)).setWarningMessage(Messages.BundleiOSPresenter_NO_ICONS_SPECIFIED);
        verify(view, times(1)).setWarningMessage("Missing icons");
        verify(view, times(6)).setEnabled(false);
        verify(view, times(2)).setEnabled(true);
    }

    @Test
    public void testOk() throws Exception {
        when(identityLister.listIdentities()).thenReturn(new String[] { "ident" });
        presenter.start();
        File profile = File.createTempFile("test", ".mobileprovision");
        profile.deleteOnExit();
        presenter.setProvisioningProfile(profile.getAbsolutePath());
        presenter.setIdentity("foo");
        presenter.setApplicationName("My App");
        presenter.setApplicationIdentifier("com.myapp");
        verify(view, times(5)).setEnabled(false);
        verify(view, times(1)).setEnabled(true);
    }

    @Test
    @Ignore
    public void testPackage() throws Exception {
        /*
         * NOTE: This test is disabled by default as it requires certificates in the local key-chain
         * The test is however good to have when developing/testing stuff
         */

        IOSBundler packager = new IOSBundler();

        File testIcon = File.createTempFile("test_icon", ".png");
        testIcon.deleteOnExit();

        Map<String, String> properties = new HashMap<String, String>();
        properties.put("CFBundleDisplayName", "Testing");
        properties.put("CFBundleExecutable", "test_exe");
        properties.put("CFBundleIdentifier", "com.defold.engine");
        List<String> icons = new ArrayList<String>();
        icons.add(testIcon.getAbsolutePath());
        String contentRoot = "src";
        String outputDir = "/tmp/test_package";
        packager.bundleApplication("iPhone Developer: Christian MURRAY (QXZXCL5J5G)", "../../share/engine_profile.mobileprovision", "test/test_exe", contentRoot, outputDir, icons, false, properties);
    }

}

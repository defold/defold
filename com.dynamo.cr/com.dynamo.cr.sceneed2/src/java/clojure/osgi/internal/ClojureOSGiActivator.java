package clojure.osgi.internal;

import org.eclipse.ui.IStartup;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

import clojure.osgi.ClojureHelper;

public class ClojureOSGiActivator implements BundleActivator, IStartup {
	public static final String PLUGIN_ID = "com.dynamo.cr.sceneed2"; //$NON-NLS-1$

	private ExtenderTracker tracker;

	@Override
	public void start(BundleContext context) throws Exception {
		System.out.println("ClojureOSGiActivator.start()");

		ClassLoader clojureClassLoader = ClojureOSGiActivator.class.getClassLoader();
		ClassLoader priorClassLoader = Thread.currentThread().getContextClassLoader();

		try {
			Thread.currentThread().setContextClassLoader(clojureClassLoader);
			ClojureOSGi.initialize(context);
			tracker = new ExtenderTracker(context);
			tracker.open();
		} finally {
			Thread.currentThread().setContextClassLoader(priorClassLoader);
		}
	}

	@Override
	public void stop(BundleContext context) throws Exception {
		tracker.close();
	}

	@Override
	public void earlyStartup() {
		System.out.println("ClojureOSGiActivator.earlyStartup()");
		
		ClojureHelper.require("internal.system");
        ClojureHelper.invoke("internal.system", "start");
	}
}

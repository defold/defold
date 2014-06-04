package clojure.osgi.internal;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class ClojureOSGiActivator implements BundleActivator {
    private ExtenderTracker tracker;

    @Override
    public void start(BundleContext context) throws Exception {
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
}

package com.dynamo.cr.clojure_eclipse;

import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;
import org.osgi.framework.BundleEvent;
import org.osgi.framework.BundleListener;

/**
 * The activator class controls the plug-in life cycle
 */
public class ClojureEclipse extends AbstractUIPlugin {
  // The plug-in ID
	public static final String PLUGIN_ID = "com.dynamo.cr.clojure-eclipse"; //$NON-NLS-1$

	// The shared instance
	private static ClojureEclipse plugin;

  protected ITracer tracer = NullTracer.INSTANCE;
	
	public ClojureEclipse() {
	}

	public void start(BundleContext context) throws Exception {
		super.start(context);
		plugin = this;
		
		context.addBundleListener(new BundleListener() {
      
      @Override
      public void bundleChanged(BundleEvent event) {
        if(event.getBundle() == ClojureEclipse.this.getBundle() && event.getType() == BundleEvent.STARTED) {
          BundleContext bundleContext = event.getBundle().getBundleContext();
          startTracing(bundleContext);
          startClojure(bundleContext);
        }
      }
    });
	}

	private void startTracing(BundleContext context) {
    tracer = new Tracer(context, TraceOptions.getTraceOptions());
    tracer.trace(TraceOptions.LOG_INFO, "Tracing started");
	}
	
	private void startClojure(BundleContext context) {
	  ClojureOSGi.require(context.getBundle(), "clojure.core");
	}
	
	public void stop(BundleContext context) throws Exception {
		plugin = null;
		super.stop(context);
	}

	/**
	 * Returns the shared instance
	 *
	 * @return the shared instance
	 */
	public static ClojureEclipse getDefault() {
		return plugin;
	}

  public static ITracer getTracer() {
    return getDefault().tracer;
  }

}


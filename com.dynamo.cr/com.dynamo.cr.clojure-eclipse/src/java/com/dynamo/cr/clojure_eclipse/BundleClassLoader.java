package com.dynamo.cr.clojure_eclipse;

import java.net.URL;

import org.osgi.framework.Bundle;

public class BundleClassLoader extends ClassLoader {
	private Bundle bundle;
  private ITracer tracer;

	public BundleClassLoader(Bundle bundle, ITracer tracer) {
		this.bundle = bundle;
		this.tracer = tracer;
	}

	@Override
	protected Class<?> findClass(String name) throws ClassNotFoundException {
	  tracer.trace(TraceOptions.LOG_INFO, "Resolving " + name + " within " + bundle.getSymbolicName());
		return bundle.loadClass(name);
	}

	@Override
	public URL getResource(String name) {
    tracer.trace(TraceOptions.LOG_INFO, "Resolving resource " + name + " within " + bundle.getSymbolicName());
		return bundle.getResource(name);
	}
}
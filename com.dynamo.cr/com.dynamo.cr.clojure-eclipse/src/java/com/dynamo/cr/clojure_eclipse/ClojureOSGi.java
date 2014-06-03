package com.dynamo.cr.clojure_eclipse;

import java.util.concurrent.Callable;

import org.osgi.framework.Bundle;


public class ClojureOSGi {
  private static volatile boolean initialized;

  private synchronized static void initialize() {
    if (initialized) {
      return;
    }

    ClojureEclipse plugin = ClojureEclipse.getDefault();

    ClassLoader loader = new BundleClassLoader(plugin.getBundle(), tracer());
    ClassLoader saved = Thread.currentThread().getContextClassLoader();
    try {
      Thread.currentThread().setContextClassLoader(loader);

      // very important, uses the right classloader
      Class.forName("clojure.java.api.Clojure", true, loader);

      initialized = true;
    } catch (Exception e) {
      throw new RuntimeException("Exception while loading namespace clojure.core", e);
    } finally {
      Thread.currentThread().setContextClassLoader(saved);
    }
  }  
  
  public static <V> V inBundle(Callable<V> callable) {
    return withBundle(ClojureEclipse.getDefault().getBundle(), callable);
  }

  public synchronized static <V> V withBundle(Bundle aBundle, Callable<V> callable) throws RuntimeException {
    initialize();
    
    tracer().trace(TraceOptions.BUNDLE, "TCCL on entry is " + Thread.currentThread().getContextClassLoader());
    
    ClassLoader bundleLoader = new BundleClassLoader(aBundle, tracer());
    
    ClassLoader saved = Thread.currentThread().getContextClassLoader();
    
    try {
      Thread.currentThread().setContextClassLoader(bundleLoader);

      tracer().trace(TraceOptions.BUNDLE, "TCCL for bundle is " + Thread.currentThread().getContextClassLoader());

      return callable.call();
    } catch (Exception e) {
      throw new RuntimeException("Exception while executing code from bundle " + aBundle.getSymbolicName(), e);
    } finally {
      Thread.currentThread().setContextClassLoader(saved);
    }
  }

  public synchronized static void require(final Bundle bundle, final String namespace) {
    ClojureOSGi.withBundle(bundle, new Callable<Object>() {
      @Override
      public Object call() {
        tracer().trace(TraceOptions.BUNDLE, "TCCL inside run block of require() " + Thread.currentThread().getContextClassLoader());
        
        ClojureHelper.require(namespace);
        return null;
      }
    });
  }

  private static ITracer tracer() {
    return ClojureEclipse.getTracer();
  }
}

package clojure.osgi.internal;

import java.util.concurrent.Callable;

import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;
import org.osgi.framework.FrameworkUtil;

import clojure.lang.Compiler;
import clojure.lang.RT;
import clojure.lang.Symbol;
import clojure.lang.Var;

public class ClojureOSGi {
  static final private Var INTERN = RT.var("clojure.core", "intern");
  static final private Var REQUIRE = RT.var("clojure.core", "require");
  static final private Var WITH_BUNDLE = RT.var("clojure.osgi.core", "with-bundle*");
  static final private Var BUNDLE = RT.var("clojure.osgi.core", "*bundle*").setDynamic();

  private static boolean initialized;

  public static void initialize(final BundleContext aContext) throws Exception {
    if (!initialized) {
      RT.var("clojure.osgi.core", "*clojure-osgi-bundle*", aContext.getBundle());

      withLoader(ClojureOSGi.class.getClassLoader(), new Callable<Object>() {
        @Override
        public Object call() {
          boolean pushed = false;

          try {
        	    System.out.println("Loading clojure.core");
            REQUIRE.invoke(Symbol.intern("clojure.core"));

            // This system property exists in Clojure, but only when launched via clojure.lang.Compile
            INTERN.invoke(Symbol.intern("clojure.core"), Symbol.intern("*warn-on-reflection*"),
            		System.getProperty("clojure.compile.warn-on-reflection", "false").equals("true"));

            Var.pushThreadBindings(RT.map(BUNDLE, aContext.getBundle()));
            pushed = true;

            REQUIRE.invoke(Symbol.intern("clojure.osgi.core"));
            REQUIRE.invoke(Symbol.intern("clojure.osgi.services"));
          } catch (Exception e) {
            throw new RuntimeException("cannot initialize clojure.osgi", e);
          } finally {
            if (pushed) {
              Var.popThreadBindings();
            }
          }
          return null;
        }
      });
      initialized = true;
    }
  }

  public static <V> V withLoader(ClassLoader aLoader, Callable<V> aRunnable) throws Exception {
    try {
      Var.pushThreadBindings(RT.map(Compiler.LOADER, aLoader));
      return aRunnable.call();
    } finally {
      Var.popThreadBindings();
    }
  }

  @SuppressWarnings("unchecked")
  public static <V> V withBundle(Bundle aBundle, final Callable<V> aCode) throws Exception {
    return (V) WITH_BUNDLE.invoke(aBundle, false, aCode);
  }

  public static void require(Bundle aBundle, final String aName) {
    try {
      withBundle(aBundle, new Callable<Object>() {
        @Override
        public Object call() throws Exception {
          REQUIRE.invoke(Symbol.intern(aName));
          return null;
        }
      });
    } catch (Exception aEx) {
      aEx.printStackTrace();
      throw new RuntimeException(aEx);
    }
  }

  public static Bundle clojureBundle() {
    return FrameworkUtil.getBundle(ClojureOSGi.class);
  }
}

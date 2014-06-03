package com.dynamo.cr.clojure_eclipse;

import clojure.java.api.Clojure;
import clojure.lang.IFn;
import clojure.lang.ISeq;

/**
 * Proxy to the Clojure Java API.
 * 
 * This class should be loaded dynamically inside a classloader that has the bundle's .clj files available.
 * 
 * @author mtnygard
 */
public class ClojureHelper {
  public static final Object EMPTY_MAP = Clojure.read("{}");
  
  private static final IFn REQUIRE = var("clojure.core", "require");
  private static final IFn SEQ = var("clojure.core", "seq");

	private static IFn var(String pkg, String var) {
		return Clojure.var(pkg, var);
	}
	
	public static void require(String packageName) {
	  REQUIRE.invoke(Clojure.read(packageName));
	}

  public static Object invoke(String namespace, String function, Object... args) {
    return Clojure.var(namespace, function).applyTo((ISeq) SEQ.invoke(args));
  }
}
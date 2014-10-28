package com.dynamo.cr.sceneed2;

import java.net.URL;
import java.util.Collections;
import java.util.Enumeration;

import org.junit.Test;

import clojure.osgi.ClojureHelper;
import clojure.osgi.internal.ClojureOSGi;

public class CleanCompile {
    public static String mungeNamespace(URL url) {
        return url.toString().replaceAll("^.*/src/clj/", "").replace(".clj", "").replace('_', '-').replace("/", ".");
    }

    @Test
    public void compileAll() {
        Enumeration<URL> sources = ClojureOSGi.clojureBundle().findEntries("/src/clj", "*.clj", true);

        for (URL source : Collections.list(sources)) {
            String namespace = mungeNamespace(source);
            System.out.println("Compiling " + namespace);
            ClojureHelper.require(namespace);
        }
    }
}

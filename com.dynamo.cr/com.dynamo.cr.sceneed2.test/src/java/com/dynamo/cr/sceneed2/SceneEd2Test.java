package com.dynamo.cr.sceneed2;

import java.util.Arrays;
import java.util.List;

import mikera.cljunit.ClojureRunner;
import mikera.cljunit.ClojureTest;

import org.junit.runner.RunWith;

import clojure.osgi.ClojureHelper;

@RunWith(ClojureRunner.class)
public class SceneEd2Test extends ClojureTest {
    @Override
    public List<String> namespaces() {
        ClojureHelper.require("suite");

        String[] nsArr = (String[]) ClojureHelper.var("suite", "test-namespaces-for-junit").deref();
        return Arrays.asList(nsArr);
    }
}

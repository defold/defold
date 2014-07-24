package com.dynamo.cr.sceneed2;

import java.util.ArrayList;
import java.util.List;

import mikera.cljunit.ClojureRunner;
import mikera.cljunit.ClojureTest;

import org.junit.runner.RunWith;

@RunWith(ClojureRunner.class)
public class ClojureTests extends ClojureTest {
    @Override
    public List<String> namespaces() {
        List<String> ns = new ArrayList<String>();

        ns.add("dynamo.texture-test");
        ns.add("dynamo.project-test");
        ns.add("internal.node-test");
        ns.add("internal.value-test");
        ns.add("internal.graph.graph-test");

        return ns;
    }
}


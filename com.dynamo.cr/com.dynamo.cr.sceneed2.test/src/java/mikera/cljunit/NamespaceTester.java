package mikera.cljunit;

import java.util.ArrayList;
import java.util.Collection;

import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;

import clojure.lang.AFn;

class NamespaceTester {
    public Description d;
    public String namespace;

    public ArrayList<VarTester> children = new ArrayList<VarTester>();

    public NamespaceTester(String ns) {
        this.namespace = ns;
        d = Description.createSuiteDescription(namespace);
        Collection<String> testVars = Clojure.getTestVars(namespace);

        for (String v : testVars) {
            VarTester vt = new VarTester(namespace, v);
            d.addChild(vt.getDescription());
            children.add(vt);
        }
    }

    public Description getDescription() {
        return d;
    }

    public void runTest(final RunNotifier n) {
        Clojure.invokeNamespaceTests(n, d, namespace, new AFn() {
            @Override
            public Object invoke() {
                for (VarTester vt : children) {
                    vt.runTest(n);
                }
                return null;
            }
        });
    }
}

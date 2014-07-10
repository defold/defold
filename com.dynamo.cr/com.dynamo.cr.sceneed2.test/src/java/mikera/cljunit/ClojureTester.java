package mikera.cljunit;

import java.util.ArrayList;
import java.util.List;

import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;

class ClojureTester {
    private final Description desc;
    public List<String> namespaces;

    public ArrayList<NamespaceTester> children = new ArrayList<NamespaceTester>();

    public ClojureTester(List<String> ns) {
        this.namespaces = ns;
        desc = Description.createSuiteDescription("Clojure Tests");

        for (String s : namespaces) {
            NamespaceTester nt = new NamespaceTester(s);
            desc.addChild(nt.getDescription());
            children.add(nt);
        }
    }

    public Description getDescription() {
        return desc;
    }

    public void runTest(RunNotifier n) {
        n.fireTestStarted(desc);
        for (NamespaceTester nt : children) {
            nt.runTest(n);
        }
        n.fireTestFinished(desc);
    }
}

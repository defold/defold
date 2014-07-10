package mikera.cljunit;

import java.lang.reflect.InvocationTargetException;
import java.util.List;

import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.ParentRunner;
import org.junit.runners.model.InitializationError;

public class ClojureRunner extends ParentRunner<NamespaceTester> {
    ClojureTester clojureTester;

    public ClojureRunner(Class<ClojureTest> testClass) throws InitializationError, InstantiationException, IllegalAccessException, IllegalArgumentException, InvocationTargetException {
        super(testClass);

        clojureTester = new ClojureTester(testClass.newInstance().namespaces());
    }

    @Override
    protected Description describeChild(NamespaceTester nt) {
        return nt.getDescription();
    }

    @Override
    protected List<NamespaceTester> getChildren() {
        return clojureTester.children;
    }

    @Override
    protected void runChild(NamespaceTester vt, RunNotifier arg1) {
        vt.runTest(arg1);
    }
}

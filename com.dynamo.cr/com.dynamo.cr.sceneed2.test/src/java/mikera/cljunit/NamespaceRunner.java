package mikera.cljunit;

import java.lang.reflect.InvocationTargetException;
import java.util.List;

import org.junit.runner.Description;
import org.junit.runner.notification.RunNotifier;
import org.junit.runners.ParentRunner;
import org.junit.runners.model.InitializationError;

public class NamespaceRunner extends ParentRunner<VarTester> {
    NamespaceTester namespaceTester;

    public NamespaceRunner(Class<NamespaceTest> testClass) throws InitializationError, InstantiationException, IllegalAccessException, IllegalArgumentException, InvocationTargetException {
        super(testClass);

        namespaceTester = new NamespaceTester(testClass.newInstance().namespace());
    }

    @Override
    protected Description describeChild(VarTester vt) {
        return vt.getDescription();
    }

    @Override
    protected List<VarTester> getChildren() {
        return namespaceTester.children;
    }

    @Override
    protected void runChild(VarTester vt, RunNotifier arg1) {
        vt.runTest(arg1);
    }
}

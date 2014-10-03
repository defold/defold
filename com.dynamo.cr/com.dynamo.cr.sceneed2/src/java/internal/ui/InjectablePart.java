package internal.ui;

import javax.annotation.PostConstruct;
import javax.inject.Inject;
import javax.inject.Named;

import org.eclipse.swt.widgets.Composite;

import clojure.osgi.ClojureHelper;

public class InjectablePart {
    @Inject
    @Named("behavior")
    Object behavior;

    @PostConstruct
    public void createControls(Composite parent) {
        ClojureHelper.invoke("dynamo.parts", "create", behavior, parent);
    }
}

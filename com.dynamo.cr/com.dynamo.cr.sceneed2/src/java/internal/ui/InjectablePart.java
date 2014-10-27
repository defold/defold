package internal.ui;

import javax.annotation.PostConstruct;
import javax.annotation.PreDestroy;
import javax.inject.Inject;
import javax.inject.Named;

import org.eclipse.e4.ui.di.Focus;
import org.eclipse.swt.widgets.Composite;

import clojure.lang.Keyword;
import clojure.lang.RT;
import clojure.osgi.ClojureHelper;

public class InjectablePart {
    private static final String NODE_NS = "dynamo.node";

    private static final Keyword PARENT = RT.keyword(null, "parent");
    private static final Keyword CREATE = RT.keyword(null, "create");
    private static final Keyword FOCUS = RT.keyword(null, "focus");
    private static final Keyword DESTROY = RT.keyword(null, "destroy");

    @Inject
    @Named("behavior")
    Object behavior;

    @PostConstruct
    private void delegateCreate(Composite parent) {
        ClojureHelper.invoke(NODE_NS, "dispatch-message", behavior, CREATE, PARENT, parent);
    }

    @Focus
    private void delegateFocus() {
        ClojureHelper.invoke(NODE_NS, "dispatch-message", behavior, FOCUS);
    }

    @PreDestroy
    private void delegateDestroy() {
        ClojureHelper.invoke(NODE_NS, "dispatch-message", behavior, DESTROY);
    }
}

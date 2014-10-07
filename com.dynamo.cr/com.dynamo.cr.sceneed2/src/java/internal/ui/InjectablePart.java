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
    private static final String VIEWS_NS = "dynamo.views";

    private static final Keyword PARENT = RT.keyword(null, "parent");
    private static final Keyword CREATE = RT.keyword(null, "create");
    private static final Keyword FOCUS = RT.keyword(null, "focus");
    private static final Keyword DESTROY = RT.keyword(null, "destroy");

    @Inject
    @Named("behavior")
    Object behavior;

    @Inject
    @PostConstruct
    private void delegateCreate(Composite parent) {
        ClojureHelper.invoke(VIEWS_NS, "dispatch-message", CREATE, behavior, PARENT, parent);
    }

    @Focus
    private void delegateFocus() {
        ClojureHelper.invoke(VIEWS_NS, "dispatch-message", FOCUS, behavior);
    }

    @PreDestroy
    private void delegateDestroy() {
        ClojureHelper.invoke(VIEWS_NS, "dispatch-message", DESTROY, behavior);
    }
}

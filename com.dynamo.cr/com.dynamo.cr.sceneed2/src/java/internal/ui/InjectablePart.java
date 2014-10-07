package internal.ui;

import javax.annotation.PostConstruct;
import javax.annotation.PreDestroy;
import javax.inject.Inject;
import javax.inject.Named;

import org.eclipse.e4.ui.di.Focus;
import org.eclipse.swt.widgets.Composite;

import clojure.osgi.ClojureHelper;

public class InjectablePart {
    private static final String VIEWS_NS = "dynamo.views";

	@Inject
    @Named("behavior")
    Object behavior;

    @PostConstruct
    private void delegateCreate(Composite parent) {
        ClojureHelper.invoke(VIEWS_NS, "create", behavior, parent);
    }

    	@Focus
    	private void delegateFocus() {
    		ClojureHelper.invoke(VIEWS_NS, "focus", behavior);
    	}

    	@PreDestroy
    	private void delegateDestroy() {
    		ClojureHelper.invoke(VIEWS_NS,  "destroy", behavior);
    	}
}

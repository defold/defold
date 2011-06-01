package com.dynamo.cr.editor.services;

import org.eclipse.ui.services.AbstractServiceFactory;
import org.eclipse.ui.services.IServiceLocator;


public class ServiceFactory extends AbstractServiceFactory {

    public ServiceFactory() {
    }

    @SuppressWarnings("rawtypes")
    @Override
    public Object create(Class serviceInterface, IServiceLocator parentLocator,
            IServiceLocator locator) {
        if (serviceInterface.equals(IBranchService.class)) {
            Object branchService = parentLocator.getService(serviceInterface);
            if (branchService != null) {
                return branchService;
            } else {
                return new BranchService();
            }
        } else {
            return locator.getService(serviceInterface);
        }
    }

}

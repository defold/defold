package com.dynamo.cr.sceneed.core.validators;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.IValidator;
import com.dynamo.cr.properties.ValidatorUtil;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Node;

public class UniqueValidator implements IValidator<Object, Unique, IPropertyObjectWorld> {

    @Override
    public IStatus validate(Unique validationParameters, Object object, String property, Object value, IPropertyObjectWorld world) {
        Node node = (Node)object;
        Node base = node.getParent();
        Class<?> scopeClass = validationParameters.scope();
        Class<?> baseClass = validationParameters.base();
        while (base != null && !baseClass.isAssignableFrom(base.getClass())) {
            base = base.getParent();
        }
        if (base != null) {
            boolean unique = testUniqueness(base, node, property, value, scopeClass);
            if (!unique) {
                return ValidatorUtil.createStatus(object, property, IStatus.ERROR, "DUPLICATE", Messages.UniqueValidator_DUPLICATE, new Object[] {value}); //$NON-NLS-1$
            }
        }
        return Status.OK_STATUS;
    }

    private static boolean testUniqueness(Node base, Node original, String property, Object value, Class<?> scopeClass) {
        for (Node sibling : base.getChildren()) {
            if (sibling != original && scopeClass.isAssignableFrom(sibling.getClass())) {
                @SuppressWarnings("unchecked")
                IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)sibling.getAdapter(IPropertyModel.class);
                Object siblingValue = propertyModel.getPropertyValue(property);
                if (value.equals(siblingValue)) {
                    return false;
                }
                if (!testUniqueness(sibling, original, property, value, scopeClass)) {
                    return false;
                }
            }
        }
        return true;
    }
}

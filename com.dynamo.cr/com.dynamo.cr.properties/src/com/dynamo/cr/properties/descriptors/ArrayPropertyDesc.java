package com.dynamo.cr.properties.descriptors;



import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;

import com.dynamo.cr.properties.IPropertyEditor;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.PropertyDesc;

public abstract class ArrayPropertyDesc<V, T, U extends IPropertyObjectWorld> extends PropertyDesc<T, U>  {

    private int count;

    public ArrayPropertyDesc(String id, String name, String catgory, int count) {
        super(id, name, catgory);
        this.count = count;
    }

    protected abstract double[] valueToArray(V value);

    protected abstract boolean isComponentEqual(V v1, V v2, int component);

    @Override
    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot) {
        return new ArrayPropertyEditor<V, T, U>(parent, count, this);
    }

}

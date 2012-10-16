package com.dynamo.cr.properties;

import org.eclipse.core.resources.IContainer;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.forms.widgets.FormToolkit;

public interface IPropertyDesc<T, U extends IPropertyObjectWorld> {

    public IPropertyEditor<T, U> createEditor(FormToolkit toolkit, Composite parent, IContainer contentRoot);

    public String getCategory();

    public String getDescription();

    public String getName();

    public String getId();

    /**
     * Get max value of property.
     * @note if not set or if min/max isn't applicable Double.MAX_VALUE is returned
     * @return max value
     */
    public double getMax();

    /**
     * Set max value of property
     * @param max max value to set
     */
    public void setMax(double max);

    /**
     * Get min value of property.
     * @note if not set or if min/min isn't applicable -Double.MAX_VALUE is returned
     * @return min value
     */
    public double getMin();

    /**
     * Set min value of property
     * @param min min value to set
     */
    public void setMin(double min);

}

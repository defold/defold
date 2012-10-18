package com.dynamo.cr.properties.test;

import java.lang.annotation.Annotation;
import java.util.HashMap;
import java.util.Map;

import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.DynamicProperties;
import com.dynamo.cr.properties.DynamicPropertyAccessor;
import com.dynamo.cr.properties.DynamicPropertyValidator;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IValidator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.Resource;
import com.dynamo.cr.properties.descriptors.TextPropertyDesc;
import com.dynamo.cr.properties.proto.PropertiesTestProto;

@Entity(commandFactory = DummyCommandFactory.class, accessor = BeanPropertyAccessor.class)
public class DummyClass {
    @Property
    @Range(min=1, max=10000)
    int integerValue = 123;

    @Property
    double doubleValue = 456;

    @Property
    String stringValue = "foobar";

    @Property
    RGB rgbValue = new RGB(1, 2, 3);

    @Property
    Vector4d vector4Value = new Vector4d(1, 2, 3, 4);

    @Property
    PropertiesTestProto.EnumType enumValue = PropertiesTestProto.EnumType.VAL_B;

    @Property
    int missingGetterSetter;

    @Property
    int notEditable;

    @Property
    int hidden;

    @Property
    @NotEmpty(severity=IStatus.ERROR)
    String requiredString = "";

    @Property
    @NotEmpty(severity=IStatus.INFO)
    String optionalString = "";

    @Property
    @Resource
    String resource = "";

    @Property(editorType = EditorType.DROP_DOWN)
    String optionsValue = "";

    String dynamicStringProp = "prop1 value";
    int dynamicIntProp = 123;
    Integer dynamicOverrideProp;

    public boolean isNotEditableEditable() {
        return false;
    }

    public boolean isHiddenVisible() {
        return false;
    }

    Map<String, IStatus> statusMap = new HashMap<String, IStatus>();

    protected IStatus validateStringValue() {
        if (stringValue.equals("invalid_string")) {
            return new Status(IStatus.ERROR, "com.dynami", "Must not be 'invalid_string'");
        } else {
            return Status.OK_STATUS;
        }
    }

    public int getIntegerValue() {
        return integerValue;
    }

    public IStatus getStatus(String property) {
        return statusMap.get(property);
    }

    public void setStatus(String property, IStatus status) {
        statusMap.put(property, status);
    }

    public void setIntegerValue(int integerValue) {
        this.integerValue = integerValue;
    }

    public double getDoubleValue() {
        return doubleValue;
    }

    public void setDoubleValue(double doubleValue) {
        this.doubleValue = doubleValue;
    }

    public String getStringValue() {
        return stringValue;
    }

    public void setStringValue(String stringValue) {
        this.stringValue = stringValue;
    }

    public RGB getRgbValue() {
        return rgbValue;
    }

    public void setRgbValue(RGB rgbValue) {
        this.rgbValue = rgbValue;
    }

    public PropertiesTestProto.EnumType getEnumValue() {
        return enumValue;
    }

    public void setEnumValue(PropertiesTestProto.EnumType enumValue) {
        this.enumValue = enumValue;
    }

    public Vector4d getVector4Value() {
        return vector4Value;
    }

    public void setVector4Value(Vector4d vector4Value) {
        this.vector4Value = vector4Value;
    }

    public String getRequiredString() {
        return this.requiredString;
    }

    public void setRequiredString(String requiredString) {
        this.requiredString = requiredString;
    }

    public String getOptionalString() {
        return this.optionalString;
    }

    public void setOptionalString(String optionalString) {
        this.optionalString = optionalString;
    }

    public String getResource() {
        return this.resource;
    }

    public void setResource(String resource) {
        this.resource = resource;
    }

    public String getOptionsValue() {
        return optionsValue;
    }

    public void setOptionsValue(String optionsValue) {
        this.optionsValue = optionsValue;
    }

    public Object[] getOptionsValueOptions() {
        return new String[] { "foo", "bar" };
    }

    @SuppressWarnings("unchecked")
    @DynamicProperties
    public IPropertyDesc<DummyClass, DummyWorld>[] getDynamicProperties() {
        IPropertyDesc<DummyClass, DummyWorld> p1 = new TextPropertyDesc<DummyClass, DummyWorld>("prop1", "Prop1", "", EditorType.DEFAULT);
        return new IPropertyDesc[] { p1 };
    }

    class Accessor implements IPropertyAccessor<DummyClass, DummyWorld> {

        @Override
        public void setValue(DummyClass obj, String property, Object value,
                DummyWorld world) {
            if (property.equals("dynamicStringProp")) {
                dynamicStringProp = (String) value;
            } else if (property.equals("dynamicIntProp")) {
                dynamicIntProp = (Integer) value;
            } else if (property.equals("dynamicOverrideProp")) {
                dynamicOverrideProp = (Integer) value;
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }

        }

        @Override
        public Object getValue(DummyClass obj, String property, DummyWorld world) {
            if (property.equals("dynamicStringProp")) {
                return dynamicStringProp;
            } else if (property.equals("dynamicIntProp")) {
                return dynamicIntProp;
            } else if (property.equals("dynamicOverrideProp")) {
                if (dynamicOverrideProp != null) {
                    return dynamicOverrideProp;
                } else {
                    return -1;
                }
            } else {
                throw new RuntimeException(String.format("No such property %s", property));
            }
        }

        @Override
        public boolean isEditable(DummyClass obj, String property,
                DummyWorld world) {
            return true;
        }

        @Override
        public boolean isVisible(DummyClass obj, String property,
                DummyWorld world) {
            return true;
        }

        @Override
        public Object[] getPropertyOptions(DummyClass obj, String property,
                DummyWorld world) {
            return new Object[0];
        }

        @Override
        public boolean isOverridden(DummyClass obj, String property, DummyWorld world) {
            if (property.equals("dynamicOverrideProp")) {
                return dynamicOverrideProp != null;
            } else {
                return false;
            }
        }

        @Override
        public void resetValue(DummyClass obj, String property, DummyWorld world) {
            if (property.equals("dynamicOverrideProp")) {
                dynamicOverrideProp = null;
            }
        }

    }

    @DynamicPropertyAccessor
    public IPropertyAccessor<DummyClass, DummyWorld> getDynamicAccessor(DummyWorld world) {
        return new Accessor();
    }

    class Validator implements IValidator<Object, Annotation, DummyWorld> {

        @Override
        public IStatus validate(Annotation validationParameters, Object object,
                String property, Object value, DummyWorld world) {
            if (property.equals("dynamicIntProp")) {
                Number numberValue = (Number) value;

                if (numberValue.doubleValue() < 0) {
                    return new Status(IStatus.ERROR, "test", "Property out of range");
                }
            }
            return Status.OK_STATUS;
        }

    }

    @DynamicPropertyValidator
    public IValidator<Object, Annotation, DummyWorld> getDynamicValidator(DummyWorld world) {
        return new Validator();
    }

}

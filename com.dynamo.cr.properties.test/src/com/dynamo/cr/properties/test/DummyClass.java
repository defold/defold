package com.dynamo.cr.properties.test;

import java.util.HashMap;
import java.util.Map;

import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.RGB;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.Resource;
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
    @NotEmpty(severity=IStatus.ERROR)
    String requiredString = "";

    @Property
    @NotEmpty(severity=IStatus.INFO)
    String optionalString = "";

    @Property
    @Resource
    String resource = "";

    public boolean isNotEditableEditable() {
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

}
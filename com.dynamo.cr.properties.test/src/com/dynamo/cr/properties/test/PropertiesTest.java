package com.dynamo.cr.properties.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.util.HashMap;
import java.util.Map;

import javax.vecmath.Vector4d;

import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.graphics.RGB;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.properties.BeanPropertyAccessor;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.properties.Range;
import com.dynamo.cr.properties.proto.PropertiesTestProto;

public class PropertiesTest {

    static class TestWorld implements IPropertyObjectWorld {
        Map<String, Integer> commandsCreated = new HashMap<String, Integer>();
        int totalCommands = 0;
    }

    static public class TestCommandFactory implements ICommandFactory<TestClass, TestWorld> {

        @Override
        public IUndoableOperation create(TestClass obj, String property,
                IPropertyAccessor<TestClass, TestWorld> accessor,
                Object oldValue, Object newValue, TestWorld world) {

            // Increase total number of commands created
            ++world.totalCommands;

            // Increase command count for this property
            Integer count = world.commandsCreated.get(property);
            if (count == null)
                count = 0;
            count++;
            world.commandsCreated.put(property, count);

            // Set actual value
            try {
                accessor.setValue(obj, property, newValue, world);
            } catch (Throwable e) {
                throw new RuntimeException(e);
            }

            return null;
        }

        @Override
        public void execute(IUndoableOperation operation, TestWorld world) {
        }
    }

    @Entity(commandFactory = TestCommandFactory.class, accessor = BeanPropertyAccessor.class)
    static public class TestClass {
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

    }

    private TestWorld world;
    private TestClass test;
    private PropertyIntrospectorModel<TestClass, TestWorld> source;
    private static PropertyIntrospector<TestClass, TestWorld> introspector = new PropertyIntrospector<TestClass, TestWorld>(TestClass.class);

    @Before
    public void setUp() {
        world = new TestWorld();
        test = new TestClass();
        source = new PropertyIntrospectorModel<TestClass, TestWorld>(test, world, introspector);
    }

    @Test
    public void testGet() throws Exception {
        assertEquals(test.getIntegerValue(), source.getPropertyValue("integerValue"));
        assertEquals(test.getDoubleValue(), source.getPropertyValue("doubleValue"));
        assertEquals(test.getStringValue(), source.getPropertyValue("stringValue"));
        assertEquals(test.getRgbValue(), source.getPropertyValue("rgbValue"));
        assertEquals(test.getVector4Value(), source.getPropertyValue("vector4Value"));
        assertEquals(test.getEnumValue(), source.getPropertyValue("enumValue"));
    }

    @Test(expected=RuntimeException.class)
    public void testGetMissingGetter() throws Exception {
        source.getPropertyValue("noSuchProperty");
    }

    void doTestSet(IPropertyModel<TestClass, TestWorld> testSource, String property, Object newValue) {
        assertEquals(null, world.commandsCreated.get(property));
        testSource.setPropertyValue(property, newValue);
        assertEquals(new Integer(1), world.commandsCreated.get(property));
        assertEquals(newValue, testSource.getPropertyValue(property));
    }

    @Test
    public void testSet() throws Exception {
        int totalCommands = 0;

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "integerValue", 1010);

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "doubleValue", 8899.0);

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "stringValue", "myNewValue");

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "vector4Value", new Vector4d(9, 8, 7, 6));

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "enumValue", PropertiesTestProto.EnumType.VAL_A);

        assertEquals(totalCommands++, world.totalCommands);
    }

    // Currently RuntimeException is thrown. This behavior might change in the future
    @Test(expected = RuntimeException.class)
    public void testMissingGetter() throws Exception {
        source.getPropertyValue("missingGetterSetter");
    }

    // Currently RuntimeException is thrown. This behavior might change in the future
    @Test(expected = RuntimeException.class)
    public void testMissingSetter() throws Exception {
        source.setPropertyValue("missingGetterSetter", 123);
    }

    @Test
    public void testRangeValidation() throws Exception {
        source.setPropertyValue("integerValue", 1);
        assertTrue(source.getPropertyStatus("integerValue").isOK());

        source.setPropertyValue("integerValue", 0);
        assertEquals(IStatus.ERROR, source.getPropertyStatus("integerValue").getSeverity());

        source.setPropertyValue("integerValue", 1000);
        assertTrue(source.getPropertyStatus("integerValue").isOK());

        source.setPropertyValue("integerValue", 10001);
        assertEquals(IStatus.ERROR, source.getPropertyStatus("integerValue").getSeverity());

        source.setPropertyValue("integerValue", 1);
        assertTrue(source.getPropertyStatus("integerValue").isOK());
    }

    @Test
    public void testMethodValidator() throws Exception {
        source.setPropertyValue("stringValue", "test");
        assertTrue(source.getPropertyStatus("stringValue").isOK());

        source.setPropertyValue("stringValue", "invalid_string");
        assertEquals(IStatus.ERROR, source.getPropertyStatus("stringValue").getSeverity());

        source.setPropertyValue("stringValue", "test2");
        assertTrue(source.getPropertyStatus("stringValue").isOK());
    }

    @Test
    public void testInitialValue() throws Exception {
        test.integerValue = 100000;
        assertTrue(!source.getPropertyStatus("integerValue").isOK());

        source.setPropertyValue("integerValue", 1);
        assertTrue(source.getPropertyStatus("integerValue").isOK());
    }
}

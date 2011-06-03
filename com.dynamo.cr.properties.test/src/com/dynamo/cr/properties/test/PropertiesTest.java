package com.dynamo.cr.properties.test;

import static org.junit.Assert.assertEquals;

import java.util.HashMap;
import java.util.Map;

import javax.vecmath.Vector4d;

import org.eclipse.swt.graphics.RGB;
import org.eclipse.ui.views.properties.IPropertySource;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.properties.ICommandFactory;
import com.dynamo.cr.properties.IPropertyAccessor;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorSource;
import com.dynamo.cr.properties.Vector4dEmbeddedSource;
import com.dynamo.cr.properties.proto.PropertiesTestProto;

public class PropertiesTest {

    static class TestWorld implements IPropertyObjectWorld {
        Map<String, Integer> commandsCreated = new HashMap<String, Integer>();
        int totalCommands = 0;
    }

    static public class TestCommandFactory implements ICommandFactory<TestClass, TestWorld> {

        @Override
        public void createCommand(TestClass obj, String property,
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
        }
    }

    static public class TestClass {
        @Property(commandFactory = TestCommandFactory.class)
        int integerValue = 123;

        @Property(commandFactory = TestCommandFactory.class)
        double doubleValue = 456;

        @Property(commandFactory = TestCommandFactory.class)
        String stringValue = "foobar";

        @Property(commandFactory = TestCommandFactory.class)
        RGB rgbValue = new RGB(1, 2, 3);

        @Property(commandFactory = TestCommandFactory.class)
        Vector4d vector4Value = new Vector4d(1, 2, 3, 4);

        @Property(embeddedSource = Vector4dEmbeddedSource.class, commandFactory = TestCommandFactory.class)
        Vector4d vector4ValueEmbedded = new Vector4d(10, 20, 30, 40);

        @Property(commandFactory = TestCommandFactory.class)
        PropertiesTestProto.EnumType enumValue = PropertiesTestProto.EnumType.VAL_B;

        @Property(commandFactory = TestCommandFactory.class)
        int missingGetterSetter;

        public int getIntegerValue() {
            return integerValue;
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

        public Vector4d getVector4ValueEmbedded() {
            return vector4ValueEmbedded;
        }

        public void setVector4ValueEmbedded(Vector4d vector4ValueEmbedded) {
            this.vector4ValueEmbedded = vector4ValueEmbedded;
        }
    }

    private TestWorld world;
    private TestClass test;
    private PropertyIntrospectorSource<TestClass, TestWorld> source;

    @Before
    public void setUp() {
        world = new TestWorld();
        test = new TestClass();
        source = new PropertyIntrospectorSource<TestClass, TestWorld>(test, world, null);
    }

    @Test
    public void testGet() throws Exception {
        assertEquals(test.getIntegerValue(), source.getPropertyValue("integerValue"));
        assertEquals(test.getDoubleValue(), source.getPropertyValue("doubleValue"));
        assertEquals(test.getStringValue(), source.getPropertyValue("stringValue"));
        assertEquals(test.getRgbValue(), source.getPropertyValue("rgbValue"));
        assertEquals(null, source.getPropertyValue("noSuchProperty"));
        assertEquals(test.getVector4Value(), source.getPropertyValue("vector4Value"));
        assertEquals(test.getEnumValue(), source.getPropertyValue("enumValue"));


        // vector4ValueEmbedded has sub-properties due to embeddedSource = Vector4dEmbeddedSource.class
        // test sub-properties here.
        IPropertySource embeddedSource = (IPropertySource)  source.getPropertyValue("vector4ValueEmbedded");
        Vector4d v = test.getVector4ValueEmbedded();
        assertEquals(v.getX(), embeddedSource.getPropertyValue("x"));
        assertEquals(v.getY(), embeddedSource.getPropertyValue("y"));
        assertEquals(v.getZ(), embeddedSource.getPropertyValue("z"));
        assertEquals(v.getW(), embeddedSource.getPropertyValue("w"));
    }

    void doTestSet(IPropertySource testSource, String property, Object newValue) {
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

        IPropertySource embeddedSource = (IPropertySource)  source.getPropertyValue("vector4ValueEmbedded");

        assertEquals(totalCommands++, world.totalCommands);
        // We can't us doTestSet here as the actual property set i the parent property and not x
        assertEquals(null, world.commandsCreated.get("vector4ValueEmbedded"));
        embeddedSource.setPropertyValue("x", 11.0);
        assertEquals(new Integer(1), world.commandsCreated.get("vector4ValueEmbedded"));
        assertEquals(11.0, embeddedSource.getPropertyValue("x"));

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


}

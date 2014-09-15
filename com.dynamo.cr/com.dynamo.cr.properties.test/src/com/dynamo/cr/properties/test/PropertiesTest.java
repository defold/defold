package com.dynamo.cr.properties.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;

import javax.vecmath.Vector4d;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.osgi.util.NLS;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.properties.IPropertyDesc;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.properties.proto.PropertiesTestProto;
import com.dynamo.cr.properties.test.subpackage.SubDummyClass;

public class PropertiesTest {

    private DummyWorld world;
    private DummyClass dummy;
    private PropertyIntrospectorModel<DummyClass, DummyWorld> source;
    private static PropertyIntrospector<DummyClass, DummyWorld> introspector = new PropertyIntrospector<DummyClass, DummyWorld>(DummyClass.class);

    private TinyDummyClass tinyDummy;
    private PropertyIntrospectorModel<TinyDummyClass, DummyWorld> tinySource;
    private static PropertyIntrospector<TinyDummyClass, DummyWorld> tinyIntrospector = new PropertyIntrospector<TinyDummyClass, DummyWorld>(TinyDummyClass.class);


    private TinyDummyClass2 tinyDummy2;
    private PropertyIntrospectorModel<TinyDummyClass2, DummyWorld> tinySource2;
    private static PropertyIntrospector<TinyDummyClass2, DummyWorld> tinyIntrospector2 = new PropertyIntrospector<TinyDummyClass2, DummyWorld>(TinyDummyClass2.class);

    @Before
    public void setUp() {
        world = new DummyWorld();
        dummy = new DummyClass();
        tinyDummy = new TinyDummyClass();
        source = new PropertyIntrospectorModel<DummyClass, DummyWorld>(dummy, world, introspector);
        tinySource = new PropertyIntrospectorModel<TinyDummyClass, DummyWorld>(tinyDummy, world, tinyIntrospector);
        tinyDummy2 = new TinyDummyClass2();
        tinySource2 = new PropertyIntrospectorModel<TinyDummyClass2, DummyWorld>(tinyDummy2, world, tinyIntrospector2);
    }

    @Test
    public void testGet() throws Exception {
        assertEquals(dummy.getIntegerValue(), source.getPropertyValue("integerValue")); //$NON-NLS-1$
        assertEquals(dummy.getDoubleValue(), source.getPropertyValue("doubleValue")); //$NON-NLS-1$
        assertEquals(dummy.getStringValue(), source.getPropertyValue("stringValue")); //$NON-NLS-1$
        assertEquals(dummy.getRgbValue(), source.getPropertyValue("rgbValue")); //$NON-NLS-1$
        assertEquals(dummy.getVector4Value(), source.getPropertyValue("vector4Value")); //$NON-NLS-1$
        assertEquals(dummy.getEnumValue(), source.getPropertyValue("enumValue")); //$NON-NLS-1$
    }

    @Test
    public void testIsOverridden() throws Exception {
        assertFalse(source.isPropertyOverridden("integerValue")); //$NON-NLS-1$
        assertFalse(source.isPropertyOverridden("stringValue")); //$NON-NLS-1$
        assertFalse(source.isPropertyOverridden("rgbValue")); //$NON-NLS-1$
        assertFalse(source.isPropertyOverridden("vector4Value")); //$NON-NLS-1$
        assertFalse(source.isPropertyOverridden("enumValue")); //$NON-NLS-1$
    }

    @Test(expected=RuntimeException.class)
    public void testGetMissingGetter() throws Exception {
        source.getPropertyValue("noSuchProperty"); //$NON-NLS-1$
    }

    void doTestSet(IPropertyModel<DummyClass, DummyWorld> dummySource, String property, Object newValue) {
        assertEquals(null, world.commandsCreated.get(property));
        dummySource.setPropertyValue(property, newValue);
        assertEquals(new Integer(1), world.commandsCreated.get(property));
        assertEquals(newValue, dummySource.getPropertyValue(property));
    }

    @SuppressWarnings({"rawtypes"})
    boolean doMultiSetTest(IPropertyModel[] propertyModels, String property, Object newValue) {
        IPropertyModel first = propertyModels[0];
        for (int j=1; j<propertyModels.length; ++j) {
            IPropertyModel current = propertyModels[j];
            if (null != current) {
                // Duplicates filtering in FormPropertySheetViewer
                IPropertyDesc[] lhs = first.getPropertyDescs();
                IPropertyDesc[] rhs = current.getPropertyDescs();

                if (lhs.length != rhs.length) {
                    return false;
                }

                for (int i = 0; i < lhs.length; ++i) {
                    IPropertyDesc lhsDesc = lhs[i];
                    IPropertyDesc rhsDesc = rhs[i];
                    if (!lhsDesc.getClass().equals(rhs[i].getClass()) || !lhsDesc.getId().equals(rhsDesc.getId())) {
                        return false;
                    }
                }
            }
        }

        for (int j=0; j<propertyModels.length; ++j) {
            IPropertyModel m = propertyModels[j];
            if (null != m) {
                m.setPropertyValue(property, newValue);
                assertEquals(new Integer(j + 1), world.commandsCreated.get(property));
                assertEquals(newValue, m.getPropertyValue(property));
            }
        }

        return true;
    }

    @Test
    @SuppressWarnings({"rawtypes"})
    public void testMultiSet() throws Exception {
        boolean result;
        IPropertyModel[] models = new IPropertyModel[2];

        models[0] = tinySource;
        models[1] = tinySource2;

        result = doMultiSetTest(models, "integerValueA", 41);
        assertEquals(result, false);

        TinyDummyClass2 tinyDummy2a = new TinyDummyClass2();
        PropertyIntrospectorModel<TinyDummyClass2, DummyWorld> tinySource2a = new PropertyIntrospectorModel<TinyDummyClass2, DummyWorld>(tinyDummy2a, world, tinyIntrospector2);
        models[0] = tinySource2a;

        result = doMultiSetTest(models, "integerValueA", 42);
        assertEquals(result, true);
    }

    @Test
    public void testSet() throws Exception {
        int totalCommands = 0;

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "integerValue", 1010); //$NON-NLS-1$

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "doubleValue", 8899.0); //$NON-NLS-1$

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "stringValue", "myNewValue"); //$NON-NLS-1$ //$NON-NLS-2$

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "vector4Value", new Vector4d(9, 8, 7, 6)); //$NON-NLS-1$

        assertEquals(totalCommands++, world.totalCommands);
        doTestSet(source, "enumValue", PropertiesTestProto.EnumType.VAL_A); //$NON-NLS-1$

        assertEquals(totalCommands++, world.totalCommands);
    }

    @Test
    public void testEditable() throws Exception {
        assertTrue(source.isPropertyEditable("integerValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyEditable("doubleValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyEditable("stringValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyEditable("rgbValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyEditable("vector4Value")); //$NON-NLS-1$
        assertTrue(source.isPropertyEditable("enumValue")); //$NON-NLS-1$
        assertFalse(source.isPropertyEditable("notEditable")); //$NON-NLS-1$
    }

    @Test
    public void testVisible() throws Exception {
        assertTrue(source.isPropertyVisible("integerValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyVisible("doubleValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyVisible("stringValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyVisible("rgbValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyVisible("vector4Value")); //$NON-NLS-1$
        assertTrue(source.isPropertyVisible("enumValue")); //$NON-NLS-1$
        assertTrue(source.isPropertyVisible("notEditable")); //$NON-NLS-1$
        assertFalse(source.isPropertyVisible("hidden")); //$NON-NLS-1$
    }

    @Test
    public void testOptions() throws Exception {
        assertArrayEquals(new String[] {"foo", "bar"}, source.getPropertyOptions("optionsValue")); //$NON-NLS-1$
        assertArrayEquals(new String[] {}, source.getPropertyOptions("doesNotExits")); //$NON-NLS-1$
    }

    @Test
    public void testDynamicProperties() throws Exception {
        assertEquals("prop1 value", source.getPropertyValue("dynamicStringProp"));
        assertThat(source.getPropertyStatus("dynamicStringProp").getSeverity(), is(IStatus.OK));
        source.setPropertyValue("dynamicStringProp", "new value");
        assertThat(source.getPropertyStatus("dynamicStringProp").getSeverity(), is(IStatus.OK));
        assertEquals("new value", source.getPropertyValue("dynamicStringProp"));

        assertEquals(123, source.getPropertyValue("dynamicIntProp"));
        assertThat(source.getPropertyStatus("dynamicIntProp").getSeverity(), is(IStatus.OK));

        source.setPropertyValue("dynamicIntProp", 456);
        assertThat(source.getPropertyStatus("dynamicIntProp").getSeverity(), is(IStatus.OK));
        assertEquals(456, source.getPropertyValue("dynamicIntProp"));

        source.setPropertyValue("dynamicIntProp", -100);
        assertEquals(-100, source.getPropertyValue("dynamicIntProp"));
        assertThat(source.getPropertyStatus("dynamicIntProp").getSeverity(), is(IStatus.ERROR));

        source.setPropertyValue("dynamicIntProp", 1000);
        assertEquals(1000, source.getPropertyValue("dynamicIntProp"));
        assertThat(source.getPropertyStatus("dynamicIntProp").getSeverity(), is(IStatus.OK));
    }

    @Test
    public void testDynamicOverride() {
        assertEquals(-1, source.getPropertyValue("dynamicOverrideProp"));
        assertFalse(source.isPropertyOverridden("dynamicOverrideProp"));
        source.setPropertyValue("dynamicOverrideProp", 1);
        assertEquals(1, source.getPropertyValue("dynamicOverrideProp"));
        assertTrue(source.isPropertyOverridden("dynamicOverrideProp"));
        source.resetPropertyValue("dynamicOverrideProp");
        assertEquals(-1, source.getPropertyValue("dynamicOverrideProp"));
        assertFalse(source.isPropertyOverridden("dynamicOverrideProp"));
    }

    // Currently RuntimeException is thrown. This behavior might change in the future
    @Test(expected = RuntimeException.class)
    public void testMissingGetter() throws Exception {
        source.getPropertyValue("missingGetterSetter"); //$NON-NLS-1$
    }

    // Currently RuntimeException is thrown. This behavior might change in the future
    @Test(expected = RuntimeException.class)
    public void testMissingSetter() throws Exception {
        source.setPropertyValue("missingGetterSetter", 123); //$NON-NLS-1$
    }

    @Test
    public void testRangeValidation() throws Exception {
        source.setPropertyValue("integerValue", 1); //$NON-NLS-1$
        assertTrue(source.getPropertyStatus("integerValue").isOK()); //$NON-NLS-1$

        source.setPropertyValue("integerValue", 0); //$NON-NLS-1$
        assertEquals(IStatus.ERROR, source.getPropertyStatus("integerValue").getSeverity()); //$NON-NLS-1$

        source.setPropertyValue("integerValue", 1000); //$NON-NLS-1$
        assertTrue(source.getPropertyStatus("integerValue").isOK()); //$NON-NLS-1$

        source.setPropertyValue("integerValue", 10001); //$NON-NLS-1$
        assertEquals(IStatus.ERROR, source.getPropertyStatus("integerValue").getSeverity()); //$NON-NLS-1$

        source.setPropertyValue("integerValue", 1); //$NON-NLS-1$
        assertTrue(source.getPropertyStatus("integerValue").isOK()); //$NON-NLS-1$
    }

    @Test
    public void testMinMax() throws Exception {

        IPropertyDesc<DummyClass, DummyWorld> propertyDesc = null;
        for (IPropertyDesc<DummyClass, DummyWorld> pd : source.getPropertyDescs()) {
            if (pd.getId().equals("integerValue")) { //$NON-NLS-1$
                 propertyDesc = pd;
            }
        }
        assertEquals(1.0, propertyDesc.getMin(), 0.0);
        assertEquals(10000.0, propertyDesc.getMax(), 0.0);
    }

    @Test
    public void testMethodValidator() throws Exception {
        source.setPropertyValue("stringValue", "test"); //$NON-NLS-1$ //$NON-NLS-2$
        assertTrue(source.getPropertyStatus("stringValue").isOK()); //$NON-NLS-1$

        source.setPropertyValue("stringValue", "invalid_string"); //$NON-NLS-1$ //$NON-NLS-2$
        assertEquals(IStatus.ERROR, source.getPropertyStatus("stringValue").getSeverity()); //$NON-NLS-1$

        source.setPropertyValue("stringValue", "test2"); //$NON-NLS-1$ //$NON-NLS-2$
        assertTrue(source.getPropertyStatus("stringValue").isOK()); //$NON-NLS-1$
    }

    @Test
    public void testInitialValue() throws Exception {
        dummy.integerValue = 100000;
        assertTrue(!source.getPropertyStatus("integerValue").isOK()); //$NON-NLS-1$

        source.setPropertyValue("integerValue", 1); //$NON-NLS-1$
        assertTrue(source.getPropertyStatus("integerValue").isOK()); //$NON-NLS-1$
    }

    @Test
    public void testGetStatus() throws Exception {
        this.tinySource.setPropertyValue("integerValue", 1); //$NON-NLS-1$
        assertTrue(this.tinySource.getPropertyStatus("integerValue").isOK()); //$NON-NLS-1$
        assertTrue(this.tinySource.getStatus().isOK());

        this.tinySource.setPropertyValue("integerValue", 0); //$NON-NLS-1$
        assertEquals(IStatus.ERROR, this.tinySource.getPropertyStatus("integerValue").getSeverity()); //$NON-NLS-1$
        assertEquals(IStatus.ERROR, this.tinySource.getStatus().getSeverity());
    }

    @Test
    public void testNotEmptyStatus() throws Exception {
        IStatus requiredStatus = this.source.getPropertyStatus("requiredString");
        assertThat(requiredStatus.getSeverity(), is(IStatus.ERROR));
        assertThat(requiredStatus.getMessage(), is(com.dynamo.cr.properties.Messages.NotEmptyValidator_EMPTY));
        IStatus optionalStatus = this.source.getPropertyStatus("optionalString");
        assertThat(optionalStatus.getSeverity(), is(IStatus.INFO));
        assertThat(optionalStatus.getMessage(), is(com.dynamo.cr.properties.Messages.NotEmptyValidator_EMPTY));
    }

    @Test
    public void testRangeStatus() throws Exception {
        this.source.setPropertyValue("integerValue", 0);
        IStatus status = this.source.getPropertyStatus("integerValue");
        assertThat(status.getSeverity(), is(IStatus.ERROR));
        assertThat(status.getMessage(), is(NLS.bind(Messages.DummyClass_integerValue_OUTSIDE_RANGE, 1.0, 10000.0)));
    }

    @Test
    public void testResourceStatus() throws Exception {
        String path = "/missing_file";
        this.source.setPropertyValue("resource", path);
        IStatus status = this.source.getPropertyStatus("resource");
        assertThat(status.getSeverity(), is(IStatus.ERROR));
        assertThat(status.getMessage(), is(NLS.bind(com.dynamo.cr.properties.Messages.ResourceValidator_NOT_FOUND, path)));
    }

    @Test
    public void testSubMessages() throws Exception {
        // Setup
        SubDummyClass subDummy = new SubDummyClass();
        PropertyIntrospector<SubDummyClass, DummyWorld> introspector = new PropertyIntrospector<SubDummyClass, DummyWorld>(SubDummyClass.class);
        PropertyIntrospectorModel<SubDummyClass, DummyWorld> introspectorModel = new PropertyIntrospectorModel<SubDummyClass, DummyWorld>(subDummy, world, introspector);

        // Test
        introspectorModel.setPropertyValue("integerValue", 0); //$NON-NLS-1$
        IStatus integerStatus = introspectorModel.getPropertyStatus("integerValue"); //$NON-NLS-1$
        assertThat(integerStatus.getSeverity(), is(IStatus.ERROR));
        assertThat(integerStatus.getMessage(), is(NLS.bind(Messages.DummyClass_integerValue_OUTSIDE_RANGE, 1.0, 10000.0)));

        introspectorModel.setPropertyValue("subIntegerValue", 0); //$NON-NLS-1$
        IStatus subIntegerStatus = introspectorModel.getPropertyStatus("subIntegerValue"); //$NON-NLS-1$
        assertThat(subIntegerStatus.getSeverity(), is(IStatus.ERROR));
        assertThat(subIntegerStatus.getMessage(), is(NLS.bind(com.dynamo.cr.properties.test.subpackage.Messages.SubDummyClass_subIntegerValue_OUTSIDE_RANGE, 1.0, 10000.0)));
    }
}

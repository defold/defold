package com.dynamo.ddf.test;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

import org.junit.Test;

import com.dynamo.ddf.DDF;
import com.dynamo.ddf.proto.TestDDF.Bytes;
import com.dynamo.ddf.proto.TestDDF.NestedArray;
import com.dynamo.ddf.proto.TestDDF.NestedArraySub1;
import com.dynamo.ddf.proto.TestDDF.NestedArraySub2;
import com.dynamo.ddf.proto.TestDDF.NestedMessage;
import com.dynamo.ddf.proto.TestDDF.ScalarTypes;
import com.dynamo.ddf.proto.TestDDF.Simple01;
import com.dynamo.ddf.proto.TestDDF.Simple01Repeated;
import com.dynamo.ddf.proto.TestDDF.Simple02Repeated;
import com.dynamo.ddf.proto.TestDDF.StringRepeated;
import com.google.protobuf.ByteString;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.Descriptors.Descriptor;

public class DDFLoaderTest
{
    private static void genericCompare(Message msg, Object obj, Descriptor descriptor) throws Throwable
    {
        // Test text format
        String msg_string = DDF.printToString(obj, descriptor);
        DynamicMessage.Builder b = DynamicMessage.newBuilder(descriptor);
        TextFormat.merge(msg_string, b);
        assertEquals(msg, b.build());

        // Test wire format, ddf -> proto
        ByteArrayOutputStream os = new ByteArrayOutputStream();
        DDF.save(obj, descriptor, os);
        b = DynamicMessage.newBuilder(descriptor);
        b.mergeFrom(os.toByteArray());
        assertEquals(msg, b.build());

        // Test wire format, proto -> ddf
        os = new ByteArrayOutputStream();
        msg.writeTo(os);
        Object obj2 = DDF.load(new ByteArrayInputStream(os.toByteArray()), descriptor, obj.getClass());
        String msg_string2 = DDF.printToString(obj2, descriptor);
        DynamicMessage.Builder b2 = DynamicMessage.newBuilder(descriptor);
        TextFormat.merge(msg_string2, b2);
        assertEquals(msg, b2.build());
    }

    private <T> T saveLoad(Message m, Descriptor descriptor, Class<T> klass)
            throws IOException
    {
        File f = File.createTempFile("tmp", ".pb");
        f.deleteOnExit();

        FileWriter w = new FileWriter(f);
        w.write(TextFormat.printToString(m));
        w.close();

        return DDF.loadTextFormat(new FileReader(f), descriptor, klass);
    }

    @Test
    public final void testScalarTypes() throws Throwable
    {
        ScalarTypes.Builder b = ScalarTypes.newBuilder();
        b.setFloat(Float.MAX_VALUE);
        b.setDouble(Double.MAX_VALUE);
        b.setInt32(Integer.MAX_VALUE);
        b.setUint32(Integer.MAX_VALUE);
        b.setInt64(Long.MAX_VALUE);
        b.setUint64(Long.MAX_VALUE);
        b.setString("foo");

        ScalarTypes m1 = b.build();

        TestDDF.ScalarTypes m2 = saveLoad(m1, ScalarTypes.getDescriptor(), TestDDF.ScalarTypes.class);

        assertEquals(m1.getFloat(), m2.m_Float, 0.0);
        assertEquals(m1.getDouble(), m2.m_Double, 0.0);
        assertEquals(m1.getInt32(), m2.m_Int32);
        assertEquals(m1.getUint32(), m2.m_Uint32);
        assertEquals(m1.getInt64(), m2.m_Int64);
        assertEquals(m1.getUint64(), m2.m_Uint64);
        assertEquals(m1.getString(), m2.m_String);

        genericCompare(m1, m2, ScalarTypes.getDescriptor());
    }

    @Test
    public final void testSimple01Repeated() throws Throwable
    {
        Simple01Repeated.Builder b = com.dynamo.ddf.proto.TestDDF.Simple01Repeated
                .newBuilder();
        b.addArray(Simple01.newBuilder().setX(10).setY(20).build());
        b.addArray(Simple01.newBuilder().setX(100).setY(200).build());
        Simple01Repeated m1 = b.build();

        TestDDF.Simple01Repeated m2 = saveLoad(m1, Simple01Repeated
                .getDescriptor(), TestDDF.Simple01Repeated.class);

        assertEquals(2, m2.m_array.size());
        assertEquals(10, m2.m_array.get(0).m_x);
        assertEquals(20, m2.m_array.get(0).m_y);
        assertEquals(100, m2.m_array.get(1).m_x);
        assertEquals(200, m2.m_array.get(1).m_y);

        genericCompare(m1, m2, Simple01Repeated.getDescriptor());
    }

    @Test
    public final void testSimple02Repeated() throws Throwable
    {
        Simple02Repeated.Builder b = Simple02Repeated.newBuilder();
        b.addArray(10);
        b.addArray(20);
        b.addArray(30);

        Simple02Repeated m1 = b.build();
        TestDDF.Simple02Repeated m2 = saveLoad(m1, Simple02Repeated
                .getDescriptor(), TestDDF.Simple02Repeated.class);
        assertEquals(10, m2.m_array.get(0).intValue());
        assertEquals(20, m2.m_array.get(1).intValue());
        assertEquals(30, m2.m_array.get(2).intValue());

        genericCompare(m1, m2, Simple02Repeated.getDescriptor());
    }

    @Test
    public final void testStringRepeated() throws Throwable
    {
        StringRepeated.Builder b = StringRepeated.newBuilder();
        b.addArray("foo");
        b.addArray("bar");
        StringRepeated m1 = b.build();
        TestDDF.StringRepeated m2 = saveLoad(m1,
                StringRepeated.getDescriptor(), TestDDF.StringRepeated.class);
        assertEquals("foo", m2.m_array.get(0));
        assertEquals("bar", m2.m_array.get(1));

        genericCompare(m1, m2, StringRepeated.getDescriptor());
    }

    @Test
    public final void testNestedMessage() throws Throwable
    {
        NestedMessage.Builder b = NestedMessage.newBuilder();
        b.setN1(NestedMessage.Nested.newBuilder().setX(10));
        b.setN2(NestedMessage.Nested.newBuilder().setX(20));
        NestedMessage m1 = b.build();
        TestDDF.NestedMessage m2 = saveLoad(m1, NestedMessage.getDescriptor(),
                TestDDF.NestedMessage.class);
        assertEquals(m2.m_n1.m_x, 10);
        assertEquals(m2.m_n2.m_x, 20);

        genericCompare(m1, m2, NestedMessage.getDescriptor());
    }

    @Test
    public final void testNestedArray() throws Throwable
    {
        NestedArray.Builder b = NestedArray.newBuilder();

        NestedArraySub1.Builder b_sub1 = NestedArraySub1.newBuilder();
        b_sub1.addArray2(NestedArraySub2.newBuilder().setA(10));
        b_sub1.addArray2(NestedArraySub2.newBuilder().setA(20));
        b_sub1.setB(100);
        b_sub1.setC(200);

        b.setD(300);
        b.setE(400);
        b.addArray1(b_sub1);

        NestedArray m1 = b.build();

        TestDDF.NestedArray m2 = saveLoad(m1, NestedArray.getDescriptor(), TestDDF.NestedArray.class);

        assertEquals(300, m2.m_d);
        assertEquals(400, m2.m_e);
        assertEquals(100, m2.m_array1.get(0).m_b);
        assertEquals(200, m2.m_array1.get(0).m_c);
        assertEquals(1, m2.m_array1.size());
        assertEquals(2, m2.m_array1.get(0).m_array2.size());
        assertEquals(10, m2.m_array1.get(0).m_array2.get(0).m_a);
        assertEquals(20, m2.m_array1.get(0).m_array2.get(1).m_a);

        genericCompare(m1, m2, NestedArray.getDescriptor());
    }

    @Test
    public final void testBytes() throws Throwable
    {
        Bytes.Builder b = Bytes.newBuilder();
        b.setData(ByteString.copyFrom(new byte[] { Byte.MAX_VALUE, Byte.MIN_VALUE }));
        Bytes m1 = b.build();

        TestDDF.Bytes m2 = saveLoad(m1, Bytes.getDescriptor(), TestDDF.Bytes.class);
        assertArrayEquals(m1.getData().toByteArray(), m2.m_data);

        genericCompare(m1, m2, Bytes.getDescriptor());
    }
}

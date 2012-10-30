package com.dynamo.cr.properties.util.test;

import static org.junit.Assert.*;

import org.junit.Test;

import com.dynamo.cr.properties.util.NumberUtil;

public class NumberUtilTest {

    @Test
    public void test() {
        assertEquals("0", NumberUtil.formatDouble(0));
        assertEquals("1.2", NumberUtil.formatDouble(1.2));
        assertEquals("12.34", NumberUtil.formatDouble(12.34));
        assertEquals("0.001", NumberUtil.formatDouble(0.001));
        assertEquals("2", NumberUtil.formatDouble(2.00000003));
        assertEquals("0.123", NumberUtil.formatDouble(0.123));
        assertEquals("123.456", NumberUtil.formatDouble(123.456));
        assertEquals("1.5", NumberUtil.formatDouble(1.499999));
        assertEquals("0.0001", NumberUtil.formatDouble(0.0001));
        assertEquals("0", NumberUtil.formatDouble(0.000001));
        assertEquals("-123.456", NumberUtil.formatDouble(-123.456));
    }

}

package com.dynamo.cr.properties.util;

public class NumberUtil {

    /**
     * Format a double with up to 5 significant values and with
     * trailing decimal zeros removed
     * @param value value to format
     * @return formatted value
     */
    public static String formatDouble(double value) {
        String s = String.format("%.5f", value);

        int i = s.indexOf('.');
        if (i != -1) {
            int j = s.length() - 1;
            while (j > i && s.charAt(j) == '0') {
                --j;
            }
            if (s.charAt(j) == '.') {
                s = s.substring(0, j);
            } else {
                s = s.substring(0, j + 1);
            }
        }

        return s;
    }

    /**
     * Format a float with up to 5 significant values and with
     * trailing decimal zeros removed
     * @param value value to format
     * @return formatted value
     */
    public static String formatFloat(Float value) {
        return formatDouble(value);
    }

    /**
     * Format an integer
     * @param value value to format
     * @return formatted value
     */
    public static String formatInt(Integer value) {
        return String.format("%d", value);
    }

    /**
     * Parse a string representing a number value (double)
     * @param value represented value
     * @throws NumberFormatException
     * @return value
     */
    public static double parseDouble(String value) {
        return Double.parseDouble(value);
    }

    /**
     * Parse a string representing a number value (float)
     * @param value represented value
     * @throws NumberFormatException
     * @return value
     */
    public static float parseFloat(String value) {
        return Float.parseFloat(value);
    }

    /**
     * Parse a string representing a number value (int)
     * @param value represented value
     * @throws NumberFormatException
     * @return value
     */
    public static int parseInt(String value) {
        return Integer.parseInt(value);
    }

}

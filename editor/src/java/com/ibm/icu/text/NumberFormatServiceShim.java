package com.ibm.icu.text;

import com.ibm.icu.util.ULocale;
import org.apache.commons.lang3.NotImplementedException;

import java.util.Locale;

/**
 * Used dynamically in {@link NumberFormat#getShim()}
 */
public class NumberFormatServiceShim extends NumberFormat.NumberFormatShim {

    private static final NumberFormat NUMBER_FORMAT = new DecimalFormat("#0.##", new DecimalFormatSymbols(ULocale.ROOT));
    private static final NumberFormat PERCENT_FORMAT = new DecimalFormat("#.##%", new DecimalFormatSymbols(ULocale.ROOT));

    @Override
    Locale[] getAvailableLocales() {
        return Locale.getAvailableLocales();
    }

    @Override
    ULocale[] getAvailableULocales() {
        return ULocale.getAvailableLocales();
    }

    @Override
    Object registerFactory(NumberFormat.NumberFormatFactory numberFormatFactory) {
        throw new NotImplementedException("registerFactory");
    }

    @Override
    boolean unregister(Object o) {
        throw new NotImplementedException("unregister");
    }

    @Override
    NumberFormat createInstance(ULocale uLocale, int i) {
        if (i == NumberFormat.PERCENTSTYLE) {
            return PERCENT_FORMAT;
        } else {
            return NUMBER_FORMAT;
        }
    }
}

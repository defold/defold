// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

package com.ibm.icu.text;

import com.ibm.icu.util.ULocale;
import org.apache.commons.lang3.NotImplementedException;

import java.util.Locale;

/**
 * Used dynamically in {@link NumberFormat#getShim()}
 * We remove the original NumberFormatServiceShim class from the ICU package and
 * replace it with our own to ensure numbers are formatted uniformly in any
 * localization. Reason: localized number formatting is suitable for currency
 * contexts, but not for programming contexts.
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

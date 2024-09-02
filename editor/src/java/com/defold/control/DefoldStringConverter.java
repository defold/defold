package com.defold.control;

import clojure.lang.IFn;
import javafx.util.StringConverter;

public class DefoldStringConverter extends StringConverter<Object> {
    private final IFn toString;
    private final IFn fromString;

    public DefoldStringConverter(IFn toString) {
        this(toString, null);
    }

    public DefoldStringConverter(IFn toString, IFn fromString) {
        this.toString = toString;
        this.fromString = fromString;
    }

    @Override
    public String toString(Object o) {
        return String.valueOf(toString.invoke(o));
    }

    @Override
    public Object fromString(String s) {
        return fromString == null ? null : fromString.invoke(s);
    }
}

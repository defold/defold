package com.defold.editor.luart;

import clojure.lang.IFn;
import org.luaj.vm2.*;

public class DynamicToStringUserdata extends LuaUserdata {
    private final IFn toString;

    public DynamicToStringUserdata(IFn toString, Object obj) {
        super(obj);
        this.toString = toString;
    }

    @Override
    public String tojstring() {
        return (String) toString.invoke(m_instance);
    }
}

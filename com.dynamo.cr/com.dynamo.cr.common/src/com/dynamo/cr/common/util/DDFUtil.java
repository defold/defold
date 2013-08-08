package com.dynamo.cr.common.util;

import com.dynamo.proto.DdfExtensions;
import com.google.protobuf.Descriptors.EnumValueDescriptor;

public class DDFUtil {

    public static String getEnumValueDisplayName(EnumValueDescriptor desc) {
        return desc.getOptions().getExtension(DdfExtensions.displayName);
    }
}

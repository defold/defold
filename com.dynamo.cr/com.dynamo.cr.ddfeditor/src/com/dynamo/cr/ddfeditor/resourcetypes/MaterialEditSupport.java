package com.dynamo.cr.ddfeditor.resourcetypes;

import com.dynamo.cr.editor.core.IResourceTypeEditSupport;
import com.dynamo.proto.DdfMath.Vector4;
import com.dynamo.render.proto.Material.MaterialDesc;
import com.dynamo.render.proto.Material.MaterialDesc.ConstantType;
import com.dynamo.render.proto.Material.MaterialDesc.FilterModeMin;
import com.dynamo.render.proto.Material.MaterialDesc.FilterModeMag;
import com.dynamo.render.proto.Material.MaterialDesc.WrapMode;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Message;

public class MaterialEditSupport implements IResourceTypeEditSupport {

    public MaterialEditSupport() {
    }

    @Override
    public Message getTemplateMessageFor(Descriptor descriptor) {
        if (descriptor.getFullName().equals(MaterialDesc.Constant.getDescriptor().getFullName())) {
            return MaterialDesc.Constant.newBuilder()
                    .setType(ConstantType.CONSTANT_TYPE_USER)
                    .setName("constant_name")
                    .setValue(Vector4.newBuilder().build()).build();
        } else if (descriptor.getFullName().equals(MaterialDesc.Sampler.getDescriptor().getFullName())) {
            return MaterialDesc.Sampler.newBuilder()
                    .setName("sampler_name")
                    .setWrapU(WrapMode.WRAP_MODE_REPEAT)
                    .setWrapV(WrapMode.WRAP_MODE_REPEAT)
                    .setFilterMin(FilterModeMin.FILTER_MODE_MIN_LINEAR_MIPMAP_LINEAR)
                    .setFilterMag(FilterModeMag.FILTER_MODE_MAG_LINEAR)
                    .build();
        }
        return null;
    }

    @Override
    public String getLabelText(Message message) {
        if (message instanceof MaterialDesc.Sampler) {
            MaterialDesc.Sampler sampler = (MaterialDesc.Sampler) message;
            return sampler.getName();
        }

        return "";
    }

}

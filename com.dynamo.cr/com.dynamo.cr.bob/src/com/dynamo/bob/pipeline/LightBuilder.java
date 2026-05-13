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

package com.dynamo.bob.pipeline;

import java.io.IOException;

import org.apache.commons.io.FilenameUtils;

import com.dynamo.bob.BuilderParams;
import com.dynamo.bob.CompileExceptionError;
import com.dynamo.bob.ProtoBuilder;
import com.dynamo.bob.ProtoParams;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;
import com.dynamo.bob.fs.ResourceUtil;
import com.dynamo.gamesys.proto.DataProto.Data;
import com.dynamo.proto.DdfStruct.ListValue;
import com.dynamo.proto.DdfStruct.Struct;
import com.dynamo.proto.DdfStruct.Value;

@ProtoParams(srcClass = Data.class, messageClass = Data.class)
@BuilderParams(name="Light", inExts={".point_light", ".directional_light", ".spot_light"}, outExt=".lightc")
public class LightBuilder extends ProtoBuilder<Data.Builder> {

    private static final double MAX_LIGHT_CONE_ANGLE_DEGREES = 180.0;
    private static final String LIGHTC_EXT = ".lightc";

    public LightBuilder() {
        registerLightOutputExtensions();
    }

    private static void registerLightOutputExtensions() {
        ResourceUtil.registerMapping(".point_light", ".point_light" + LIGHTC_EXT);
        ResourceUtil.registerMapping(".directional_light", ".directional_light" + LIGHTC_EXT);
        ResourceUtil.registerMapping(".spot_light", ".spot_light" + LIGHTC_EXT);
    }

    private static Value getRequiredFieldValue(IResource resource, Struct struct, String fieldName) throws CompileExceptionError {
        Value value = struct.getFieldsMap().get(fieldName);
        if (value == null) {
            throw new CompileExceptionError(resource, 0, String.format("missing required field '%s'", fieldName));
        }
        return value;
    }

    private static double getRequiredNumberField(IResource resource, Struct struct, String fieldName) throws CompileExceptionError {
        Value value = getRequiredFieldValue(resource, struct, fieldName);
        if (value.getKindCase() != Value.KindCase.NUMBER) {
            throw new CompileExceptionError(resource, 0, String.format("field '%s' must be a number", fieldName));
        }
        return value.getNumber();
    }

    private static void validateLightColorField(IResource resource, Struct struct) throws CompileExceptionError {
        Value value = getRequiredFieldValue(resource, struct, "color");
        if (value.getKindCase() != Value.KindCase.LIST) {
            throw new CompileExceptionError(resource, 0, "field 'color' must be a list of 3 or 4 numbers");
        }

        ListValue color = value.getList();
        int colorValueCount = color.getValuesCount();
        if (colorValueCount < 3 || colorValueCount > 4) {
            throw new CompileExceptionError(resource, 0, "field 'color' must contain 3 or 4 numbers");
        }

        for (Value component : color.getValuesList()) {
            if (component.getKindCase() != Value.KindCase.NUMBER) {
                throw new CompileExceptionError(resource, 0, "field 'color' must contain only numbers");
            }
        }
    }

    private static Struct.Builder getLightDataStructBuilder(IResource resource, Data.Builder messageBuilder) throws CompileExceptionError {
        if (!messageBuilder.hasData() || messageBuilder.getData().getKindCase() != Value.KindCase.STRUCT) {
            throw new CompileExceptionError(resource, 0, "light data must contain a struct payload");
        }
        return messageBuilder.getDataBuilder().getStructBuilder();
    }

    private static void setNumberField(Struct.Builder structBuilder, String fieldName, double number) {
        structBuilder.putFields(fieldName, Value.newBuilder().setNumber(number).build());
    }

    private static void validateAndNormalizeLightData(IResource resource, Data.Builder messageBuilder, String lightTypeTag) throws CompileExceptionError {
        Struct.Builder lightDataBuilder = getLightDataStructBuilder(resource, messageBuilder);
        Struct lightData = lightDataBuilder.build();

        validateLightColorField(resource, lightData);
        double intensity = Math.max(0.0, getRequiredNumberField(resource, lightData, "intensity"));
        setNumberField(lightDataBuilder, "intensity", intensity);

        switch (lightTypeTag) {
            case "directional_light":
                return;
            case "point_light":
            {
                double range = Math.max(0.0, getRequiredNumberField(resource, lightData, "range"));
                setNumberField(lightDataBuilder, "range", range);
                return;
            }
            case "spot_light":
            {
                double range = Math.max(0.0, getRequiredNumberField(resource, lightData, "range"));
                double outerConeAngleDegrees = Math.max(0.0, Math.min(MAX_LIGHT_CONE_ANGLE_DEGREES, getRequiredNumberField(resource, lightData, "outer_cone_angle")));
                double innerConeAngleDegrees = Math.max(0.0, Math.min(outerConeAngleDegrees, getRequiredNumberField(resource, lightData, "inner_cone_angle")));
                setNumberField(lightDataBuilder, "range", range);
                setNumberField(lightDataBuilder, "inner_cone_angle", Math.toRadians(innerConeAngleDegrees));
                setNumberField(lightDataBuilder, "outer_cone_angle", Math.toRadians(outerConeAngleDegrees));
                return;
            }
            default:
                throw new CompileExceptionError(resource, 0, "unsupported light type tag: " + lightTypeTag);
        }
    }

    private static Data.Builder transformLight(IResource resource, Data.Builder messageBuilder, String... tags) throws CompileExceptionError {
        String lightTypeTag = tags[tags.length - 1];
        validateAndNormalizeLightData(resource, messageBuilder, lightTypeTag);
        messageBuilder.clearTags();
        for (String tag : tags) {
            messageBuilder.addTags(tag);
        }
        return messageBuilder;
    }

    private static String lightTypeTagFromResource(IResource resource) {
        String ext = FilenameUtils.getExtension(resource.getPath());
        switch (ext) {
            case "point_light":
                return "point_light";
            case "directional_light":
                return "directional_light";
            case "spot_light":
                return "spot_light";
            default:
                throw new IllegalArgumentException("Unsupported light resource extension: " + ext);
        }
    }

    private static String lightBuildExtFromResource(IResource resource) {
        return "." + lightTypeTagFromResource(resource) + LIGHTC_EXT;
    }

    @Override
    public Task create(IResource input) throws IOException, CompileExceptionError {
        Task.TaskBuilder taskBuilder = Task.newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(lightBuildExtFromResource(input)));
        createSubTasks(getSrcBuilder(input), taskBuilder);
        return taskBuilder.build();
    }

    @Override
    protected Data.Builder transform(Task task, IResource resource, Data.Builder messageBuilder) throws IOException, CompileExceptionError {
        return transformLight(resource, messageBuilder, "light", lightTypeTagFromResource(resource));
    }
}

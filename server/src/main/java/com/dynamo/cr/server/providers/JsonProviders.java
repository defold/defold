package com.dynamo.cr.server.providers;

import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.StringWriter;
import java.lang.annotation.Annotation;
import java.lang.reflect.Method;
import java.lang.reflect.Type;
import java.util.Iterator;
import java.util.List;

import javax.ws.rs.Consumes;
import javax.ws.rs.Produces;
import javax.ws.rs.WebApplicationException;
import javax.ws.rs.core.MediaType;
import javax.ws.rs.core.MultivaluedMap;
import javax.ws.rs.ext.MessageBodyReader;
import javax.ws.rs.ext.MessageBodyWriter;
import javax.ws.rs.ext.Provider;

import org.codehaus.jackson.JsonFactory;
import org.codehaus.jackson.JsonGenerationException;
import org.codehaus.jackson.JsonGenerator;
import org.codehaus.jackson.JsonNode;
import org.codehaus.jackson.JsonToken;
import org.codehaus.jackson.map.ObjectMapper;

import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.EnumValueDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;

public class JsonProviders {

    @Provider
    @Consumes({"application/json", "text/javascript"})
    public static class ProtobufMessageBodyReader implements MessageBodyReader<Message> {

        @Override
        public boolean isReadable(Class<?> type, Type genericType,
                Annotation[] annotations, MediaType mediaType) {
            return Message.class.isAssignableFrom(type);
        }

        @Override
        public Message readFrom(Class<Message> type,
                java.lang.reflect.Type genericType, Annotation[] annotations, MediaType mediaType,
                MultivaluedMap<String, String> httpHeaders, InputStream entityStream)
                throws IOException, WebApplicationException {
            try {
                Method newBuilder = type.getMethod("newBuilder");
                Message.Builder builder = (Message.Builder) newBuilder.invoke(type);

                ObjectMapper m = new ObjectMapper();
                JsonNode node = m.readValue(new InputStreamReader(entityStream, "UTF-8"), JsonNode.class);
                return JsonToMessage(node, builder);

            } catch (Exception e) {
                throw new WebApplicationException(e);
            }
        }

        private static Message JsonToMessage(JsonNode node, Message.Builder builder) throws IOException {
            Iterator<String> iterator = node.getFieldNames();
            Descriptor descriptor = builder.getDescriptorForType();
            while (iterator.hasNext()) {
                String name = iterator.next();

                FieldDescriptor fieldDescriptor = descriptor.findFieldByName(name);
                if (fieldDescriptor == null) {
                    throw new IOException(String.format("Unknown field %s", name));
                }

                if (fieldDescriptor.isRepeated()) {
                    JsonNode arrayNode = node.get(name);
                    for (int i = 0; i < arrayNode.size(); ++i) {
                        Object value = JsonToObject(arrayNode.get(i), builder, fieldDescriptor);
                        builder.addRepeatedField(fieldDescriptor, value);
                    }
                }
                else {
                    Object value = JsonToObject(node.get(name), builder, fieldDescriptor);
                    builder.setField(fieldDescriptor, value);
                }
            }

            return builder.build();
        }

        private static Object JsonToObject(JsonNode node, Message.Builder builder, FieldDescriptor fieldDescriptor) throws IOException {

            FieldDescriptor.JavaType type = fieldDescriptor.getJavaType();
            JsonToken token = node.asToken();

            switch (token) {
            case VALUE_STRING:
                if (type == FieldDescriptor.JavaType.ENUM) {
                    return fieldDescriptor.getEnumType().findValueByName(node.getTextValue());
                }
                else {
                    return node.getTextValue();
                }
            case VALUE_FALSE:
                return false;

            case VALUE_TRUE:
                return false;

            case VALUE_NULL:
                throw new IOException("null values not supported");

            case VALUE_NUMBER_FLOAT:
                if (type == FieldDescriptor.JavaType.FLOAT)
                    return (float) node.getValueAsDouble();
                else if (type == FieldDescriptor.JavaType.DOUBLE)
                    return node.getValueAsDouble();

            case VALUE_NUMBER_INT:
                if (type == FieldDescriptor.JavaType.INT)
                    return node.getValueAsInt();
                else if (type == FieldDescriptor.JavaType.LONG)
                    return node.getValueAsLong();

            case START_OBJECT:
                return JsonToMessage(node, builder.newBuilderForField(fieldDescriptor));
            }

            throw new RuntimeException(String.format("Unable to convert %s", node.toString()));
        }
    }

    @Provider
    @Produces({"application/json", "text/javascript"})
    public static class ProtobufMessageBodyWriter implements MessageBodyWriter<Message> {
        public boolean isWriteable(Class<?> type, Type genericType, Annotation[] annotations, MediaType mediaType) {
            return Message.class.isAssignableFrom(type);
        }

        void ObjectToJSON(Object o, JsonGenerator generator) throws JsonGenerationException, IOException {
            if (o instanceof Integer) {
                generator.writeNumber((Integer) o);
            }
            else if (o instanceof Long) {
                generator.writeNumber((Long) o);
            }
            else if (o instanceof Number) {
                generator.writeNumber((Double) o);
            }
            else if (o instanceof String) {
                generator.writeString((String) o);
            }
            else if (o instanceof Boolean) {
                generator.writeBoolean((Boolean) o);
            }
            else if (o instanceof EnumValueDescriptor) {
                generator.writeString(((EnumValueDescriptor) o).getName());
            }
            else if (o instanceof Message) {
                MessageToJSON((Message) o, generator);
            }
            else {
                throw new RuntimeException(String.format("Unsupported type: %s", o));
            }
        }

        void MessageToJSON(Message m, JsonGenerator generator) throws JsonGenerationException, IOException {
            generator.writeStartObject();
            Descriptor desc = m.getDescriptorForType();
            List<FieldDescriptor> fields = desc.getFields();
            for (FieldDescriptor fieldDescriptor : fields) {

                if (fieldDescriptor.isRepeated()) {
                    @SuppressWarnings("unchecked")
                    List<Object> list = (List<Object>) m.getField(fieldDescriptor);
                    generator.writeArrayFieldStart(fieldDescriptor.getName());
                    for (Object object : list) {
                        ObjectToJSON(object, generator);
                    }
                    generator.writeEndArray();
                }
                else {
                    generator.writeFieldName(fieldDescriptor.getName());
                    ObjectToJSON(m.getField(fieldDescriptor), generator);
                }
            }
            generator.writeEndObject();
        }

        private byte[] toBytes(Message m) {
            StringWriter writer = new StringWriter();
            JsonGenerator generator = null;
            try {
                generator = (new JsonFactory()).createJsonGenerator(writer);
                MessageToJSON(m, generator);
                generator.close();
                byte[] bytes = writer.getBuffer().toString().getBytes("UTF-8");
                return bytes;
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        }

        public long getSize(Message m, Class<?> type, Type genericType, Annotation[] annotations, MediaType mediaType) {
            return toBytes(m).length;
        }

        @Override
        public void writeTo(Message m, @SuppressWarnings("rawtypes") Class type, Type genericType, Annotation[] annotations,
                    MediaType mediaType, @SuppressWarnings("rawtypes") MultivaluedMap httpHeaders,
                    OutputStream entityStream) throws IOException, WebApplicationException {

            entityStream.write(toBytes(m));
        }
    }
}


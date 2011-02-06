mkdir -p generated
protoc --java_out=generated ../com.dynamo.cr.common/proto/cr_protocol_ddf.proto -I$DYNAMO_HOME/share/proto -I../com.dynamo.cr.common/proto -I$DYNAMO_HOME/ext/include

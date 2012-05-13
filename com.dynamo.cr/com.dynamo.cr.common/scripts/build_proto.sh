mkdir -p generated
protoc --java_out=generated proto/cr_protocol_ddf.proto -I$DYNAMO_HOME/share/proto -Iproto -I$DYNAMO_HOME/ext/include
protoc --java_out=generated proto/cr_ddf.proto -I$DYNAMO_HOME/share/proto -Iproto -I$DYNAMO_HOME/ext/include

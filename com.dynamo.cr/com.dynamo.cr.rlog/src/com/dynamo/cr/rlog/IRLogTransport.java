package com.dynamo.cr.rlog;

import com.dynamo.cr.rlog.proto.RLog.Record;

public interface IRLogTransport {

    boolean send(Record record);

}

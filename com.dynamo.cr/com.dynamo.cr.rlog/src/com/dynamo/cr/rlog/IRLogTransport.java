package com.dynamo.cr.rlog;

import java.io.IOException;

import com.dynamo.cr.rlog.proto.RLog.Record;

public interface IRLogTransport {

    /**
     * Send logging record.
     * @param entry entry to send
     * @throws IOException
     */
    void send(Record record) throws IOException;

}

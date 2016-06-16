package com.dynamo.cr.server.util;

import java.time.Instant;
import java.util.Date;

public class SystemTimeSource implements TimeSource {

    @Override
    public Date currentDate() {
        return new Date();
    }

    @Override
    public Instant currentInstant() {
        return Instant.now();
    }
}

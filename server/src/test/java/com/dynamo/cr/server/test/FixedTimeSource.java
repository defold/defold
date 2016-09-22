package com.dynamo.cr.server.test;

import com.dynamo.cr.server.util.TimeSource;

import java.time.Instant;
import java.util.Date;

public class FixedTimeSource implements TimeSource {
    private final Instant fixedTime;

    public FixedTimeSource(Instant fixedTime) {
        this.fixedTime = fixedTime;
    }

    @Override
    public Date currentDate() {
        return Date.from(fixedTime);
    }

    @Override
    public Instant currentInstant() {
        return fixedTime;
    }
}

package com.dynamo.cr.server.util;

import java.time.Instant;
import java.util.Date;

public interface TimeSource {
    Date currentDate();

    Instant currentInstant();
}

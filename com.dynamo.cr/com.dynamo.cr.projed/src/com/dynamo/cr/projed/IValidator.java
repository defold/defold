package com.dynamo.cr.projed;

import org.eclipse.core.runtime.IStatus;

public interface IValidator {
    IStatus validate(String value);
}

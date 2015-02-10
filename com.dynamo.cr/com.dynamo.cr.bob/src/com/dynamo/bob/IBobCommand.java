package com.dynamo.bob;

import java.io.IOException;
import java.util.List;

public interface IBobCommand {

    public int work();
    public List<TaskResult> execute(Project project, IProgress progress) throws IOException;
}

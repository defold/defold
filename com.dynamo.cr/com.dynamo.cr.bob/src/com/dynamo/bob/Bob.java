package com.dynamo.bob;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import org.eclipse.core.runtime.NullProgressMonitor;

public class Bob {

    public static void main(String[] args) throws IOException, CompileExceptionError {
        String buildDirectory = "build/default";
        Project project = new Project(new DefaultFileSystem(), ".", buildDirectory);

        project.scanPackage("com.dynamo.bob");
        project.scanPackage("com.dynamo.bob.pipeline");

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", buildDirectory));

        project.findSources(".", skipDirs);
        List<TaskResult> result = project.build(new NullProgressMonitor());
        boolean ret = true;
        for (TaskResult taskResult : result) {
            System.out.println(taskResult);
            if (!taskResult.isOk()) {
                ret = false;
            }
        }
        System.exit(ret ? 0 : 1);
    }
}

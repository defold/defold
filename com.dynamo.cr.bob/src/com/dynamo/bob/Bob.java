package com.dynamo.bob;

import java.io.IOException;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class Bob {

    public static void main(String[] args) throws IOException {
        String buildDirectory = "build/default";
        Project project = new Project(new DefaultFileSystem(), buildDirectory);

        project.scanPackage("com.dynamo.bob");
        project.scanPackage("com.dynamo.bob.pipeline");

        Set<String> skipDirs = new HashSet<String>(Arrays.asList(".git", buildDirectory));

        project.scan(".", skipDirs);
        List<TaskResult> result = project.build();
        int ret = 0;
        for (TaskResult taskResult : result) {
            System.out.println(taskResult);
            ret = Math.max(ret, taskResult.getReturnCode());
        }
        System.exit(ret);
    }
}

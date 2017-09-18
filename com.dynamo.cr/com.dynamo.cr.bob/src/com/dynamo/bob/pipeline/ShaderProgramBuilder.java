package com.dynamo.bob.pipeline;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.util.Locale;
import java.util.regex.Pattern;

import com.dynamo.bob.CopyBuilder;
import com.dynamo.bob.Task;
import com.dynamo.bob.fs.IResource;

public abstract class ShaderProgramBuilder extends CopyBuilder {

    private static Pattern directiveLinePattern = Pattern.compile("^\\s*(#|//).*");

    private static boolean isDirectiveLine(String line) {
        return line.isEmpty() || directiveLinePattern.matcher(line).find();
    }

    public abstract void writeExtraDirectives(PrintWriter writer);

    @Override
    public void build(Task<Void> task) throws IOException {
        IResource in = task.getInputs().get(0);
        IResource out = task.getOutputs().get(0);

        try (ByteArrayInputStream is = new ByteArrayInputStream(in.getContent());
             InputStreamReader isr = new InputStreamReader(is);
             BufferedReader reader = new BufferedReader(isr);
             ByteArrayOutputStream os = new ByteArrayOutputStream(is.available() + 128 * 16);
             PrintWriter writer = new PrintWriter(os)) {

            // Write directives from shader.
            String line = null;
            String firstNonDirectiveLine = null;
            int directiveLineCount = 0;

            while ((line = reader.readLine()) != null) {
                if (isDirectiveLine(line)) {
                    writer.println(line);
                    ++directiveLineCount;
                } else {
                    firstNonDirectiveLine = line;
                    break;
                }
            }

            // Write our directives.
            if (directiveLineCount != 0) {
                writer.println();
            }

            writeExtraDirectives(writer);
            writer.println();

            // We want "correct" line numbers from the GLSL compiler.
            //
            // Some Android devices don't like setting #line to something below 1,
            // see JIRA issue: DEF-1786.
            // We still want to have correct line reporting on most devices so
            // only output the "#line N" directive in debug builds.
            if (project.hasOption("debug")) {
                writer.printf(Locale.ROOT, "#line %d", directiveLineCount);
                writer.println();
            }

            // Write the first non-directive line from above.
            if (firstNonDirectiveLine != null) {
                writer.println(firstNonDirectiveLine);
            }

            // Write the remaining lines from the shader.
            while ((line = reader.readLine()) != null) {
                writer.println(line);
            }

            writer.flush();
            out.setContent(os.toByteArray());
        }
    }

}

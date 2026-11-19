// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

import java.io.File;
import java.util.Arrays;

import org.codehaus.jackson.map.ObjectMapper;

import com.dynamo.bob.Bob;
import com.dynamo.bob.Progress;

// Build-only entry point: reuse Bob's native tools while invoke creates and disposes
// a separate project (including settings and build options) for each command.
public class BobTestContent {
    private static int run(String manifest) throws Exception {
        String[][] commands = new ObjectMapper().readValue(new File(manifest), String[][].class);
        for (String[] command : commands) {
            System.out.println("Bob test content: " + Arrays.toString(command));
            if (!Bob.invoke(null, Progress.console(), null, command).success) {
                return 1;
            }
        }
        return 0;
    }

    public static void main(String[] args) {
        int exitCode;
        try {
            exitCode = run(args[0]);
        } catch (Bob.OptionValidationException e) {
            exitCode = e.exitCode;
        } catch (Exception e) {
            e.printStackTrace();
            exitCode = 1;
        }
        // Match Bob.main: finish background workers and native-tool cleanup once.
        System.exit(exitCode);
    }
}

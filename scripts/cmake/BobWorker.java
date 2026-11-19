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

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.lang.reflect.InvocationTargetException;
import java.nio.charset.StandardCharsets;

import org.codehaus.jackson.map.ObjectMapper;
import com.dynamo.bob.Bob;
import com.dynamo.bob.Progress;

// Build-only worker owned by the calling Python process. Each request has a fresh
// Bob project; only JVM startup, class loading and native tools are shared.
public class BobWorker {
    public static void main(String[] args) throws Exception {
        String replyPrefix = args[0];
        ObjectMapper mapper = new ObjectMapper();
        BufferedReader input = new BufferedReader(new InputStreamReader(System.in, StandardCharsets.UTF_8));
        String line;
        while ((line = input.readLine()) != null) {
            int result = 0;
            try {
                String[][] request = mapper.readValue(line, String[][].class);
                if (request[0][0].equals("com.dynamo.bob.Bob")) {
                    result = Bob.invoke(null, Progress.console(), null, request[1]).success ? 0 : 1;
                } else {
                    Class.forName(request[0][0]).getMethod("main", String[].class).invoke(null, (Object) request[1]);
                }
            } catch (Bob.OptionValidationException e) {
                result = e.exitCode;
            } catch (Exception e) {
                Throwable cause = e instanceof InvocationTargetException ? e.getCause() : e;
                cause.printStackTrace();
                result = 1;
            }
            // The unique ASCII prefix separates replies from ordinary build output,
            // including output written directly by native tools.
            System.out.println("\n" + replyPrefix + result);
            System.out.flush();
            if (result != 0) {
                System.exit(result);
            }
        }
        // Bob owns background workers; release them once, after the last request.
        System.exit(0);
    }
}

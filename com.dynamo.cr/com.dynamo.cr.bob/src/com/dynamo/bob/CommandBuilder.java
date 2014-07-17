package com.dynamo.bob;

import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import org.apache.commons.lang3.StringUtils;

import com.dynamo.bob.fs.IResource;

/**
 * Builder for external commands.
 * TODO: Only a skeleton and not yet functional
 * @author Christian Murray
 *
 */
public class CommandBuilder extends Builder<Void> {

    private BuilderParams params;
    private static Pattern varPattern = Pattern.compile("\\$\\{(\\w*?)\\}");
    private static Pattern varIndexPattern = Pattern.compile("\\$\\{(\\w*?)\\[(\\d+)\\]\\}");

    public CommandBuilder() {
        params = this.getClass().getAnnotation(BuilderParams.class);
    }

    public static void substituteVars(List<String> lst, Map<String, Object>... params) {
        int i = 0;
        for (String x : lst) {
            Matcher m = varPattern.matcher(x);
            if (m.find()) {
                String key = m.group(1);
                for (Map<String, Object> dict : params) {
                    if (dict.containsKey(key)) {
                        Object value = dict.get(key);
                        if (value instanceof String) {
                            lst.set(i, (String) value);
                        } else {
                            @SuppressWarnings("unchecked")
                            List<String> listValue = (List<String>) value;
                            lst.set(i, StringUtils.join(listValue, " "));
                        }
                    }
                }
            }
            ++i;
        }
    }

    public static void substituteIndexVars(List<String> lst, Map<String, Object>... params) {
        int i = 0;
        for (String x : lst) {
            Matcher m = varIndexPattern.matcher(x);
            if (m.find()) {
                String key = m.group(1);
                for (Map<String, Object> dict : params) {
                    if (dict.containsKey(key)) {
                        Object value = dict.get(key);
                        int index = Integer.parseInt(m.group(2));
                        if (value instanceof String) {
                            lst.set(i, (String) value);
                        } else {
                            @SuppressWarnings("unchecked")
                            List<String> listValue = (List<String>) value;
                            lst.set(i, listValue.get(index));
                        }
                    }
                }
            }
            ++i;
        }
    }

    public static List<String> substitute(String command, Map<String, Object>... params) {
        List<String> lst = Arrays.asList(command.split("\\s+"));
        substituteVars(lst, params);
        substituteIndexVars(lst, params);

        return lst;
    }

    @Override
    public Task<Void> create(IResource input) {
        return Task.<Void>newBuilder(this)
                .setName(params.name())
                .addInput(input)
                .addOutput(input.changeExt(params.outExt()))
                .build();
    }

    @Override
    public void build(Task<Void> task) {
        // TODO Auto-generated method stub

    }
}

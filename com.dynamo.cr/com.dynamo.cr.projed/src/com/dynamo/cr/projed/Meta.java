package com.dynamo.cr.projed;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;

import com.dynamo.bob.Bob;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.projed.KeyMeta.Type;

public class Meta {

    private static ArrayList<CategoryMeta> categories;

    static class NotZero implements IValidator {

        private String key;

        public NotZero(String key) {
            this.key = key;
        }

        @Override
        public IStatus validate(String value) {
            if (value != null && value.length() > 0) {
                int x = Integer.parseInt(value);
                if (x <= 0) {
                    return new Status(IStatus.ERROR, "com.dynamo.cr.editor", String.format("%s must be greater than zero", key));
                }
            }
            return Status.OK_STATUS;
        }

    }

    static class IsInteger implements IValidator {

        private String key;

        public IsInteger(String key) {
            this.key = key;
        }

        @Override
        public IStatus validate(String value) {
            if (value != null && value.length() > 0) {
                try {
                    Integer.parseInt(value);
                } catch (NumberFormatException e) {
                    return new Status(IStatus.ERROR, "com.dynamo.cr.editor", String.format("%s must be an integer", key));
                }
            }
            return Status.OK_STATUS;
        }

    }

    static class IsNumber implements IValidator {

        private String key;

        public IsNumber(String key) {
            this.key = key;
        }

        @Override
        public IStatus validate(String value) {
            if (value != null && value.length() > 0) {
                try {
                    Float.parseFloat(value);
                } catch (NumberFormatException e) {
                    return new Status(IStatus.ERROR, "com.dynamo.cr.editor", String.format("%s must be a number", key));
                }
            }
            return Status.OK_STATUS;
        }

    }

    static class ResourceExists implements IValidator {

        private String key;

        public ResourceExists(String key) {
            this.key = key;
        }

        @Override
        public IStatus validate(String value) {
            if (value != null && value.length() > 0) {

                IProject project = EditorUtil.getProject();
                if (project == null) {
                    // Due to unit tests
                    return Status.OK_STATUS;
                }

                IFolder root = EditorUtil.getContentRoot(project);
                // TODO: Hack until we compile game.project file
                if (!value.endsWith(".icns") && !value.endsWith(".ico") && !value.endsWith(".png") &&
                    !value.endsWith(".html") && !value.endsWith(".css") && !value.endsWith(".plist") && !value.endsWith(".xml") &&
                    !value.endsWith(".texture_profiles")) { // TODO: We need to treat texture_profiles as a raw file since it's not being compiled.
                    value = value.substring(0, value.length() - 1);
                }

                if (!root.getFile(new Path(value)).exists()) {
                    return new Status(IStatus.ERROR, "com.dynamo.cr.editor", String.format("Resource %s does not exists (%s)", value, key));
                }

            }
            return Status.OK_STATUS;
        }

    }

    static IValidator createValidator(String key, Type type) {
        switch (type) {
        case INTEGER:
            return new IsInteger(key);
        case NUMBER:
            return new IsNumber(key);
        case RESOURCE:
            return new ResourceExists(key);
        }

        return null;
    }

    private static void createMeta() {
        categories = new ArrayList<CategoryMeta>();

        InputStream is = Bob.class.getResourceAsStream("meta.properties");
        ProjectProperties pp = new ProjectProperties();
        try {
            pp.load(is);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }

        Map<String, Type> typeMap = new HashMap<String, KeyMeta.Type>();
        typeMap.put("string", Type.STRING);
        typeMap.put("integer", Type.INTEGER);
        typeMap.put("bool", Type.BOOLEAN);
        typeMap.put("resource", Type.RESOURCE);
        typeMap.put("number", Type.NUMBER);

        for (String category : pp.getCategoryNames()) {
            Collection<String> keys = pp.getKeys(category);
            List<KeyMeta> metaKeys = new ArrayList<KeyMeta>();
            for (String key : keys) {
                if (key.endsWith(".type")) {
                    String name = key.split("\\.")[0];
                    Type type = Type.STRING;
                    String typeString = pp.getStringValue(category, key);
                    if (typeMap.containsKey(typeString)) {
                        type = typeMap.get(typeString);
                    }

                    String defaultValue = pp.getStringValue(category, name + ".default", "");
                    String help = pp.getStringValue(category, name + ".help", "");

                    KeyMeta keyMeta = new KeyMeta(name, type, defaultValue);
                    keyMeta.setHelp(help);

                    IValidator validator = createValidator(category + "." + name, type);
                    keyMeta.setValidator(validator);

                    metaKeys.add(keyMeta);
                }
            }

            String desc = pp.getStringValue(category, "help", "");
            CategoryMeta categoryMeta = new CategoryMeta(category, desc, metaKeys);
            categories.add(categoryMeta);
        }
    }

    public static List<CategoryMeta> getDefaultMeta() {
        if (categories == null) {
            createMeta();
        }
        return categories;
    }

}

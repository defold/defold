package com.dynamo.cr.projed;

public class KeyMeta {

    public enum Type {
        STRING,
        INTEGER,
        NUMBER,
        BOOLEAN,
        RESOURCE,
    }

    private CategoryMeta category;
    private String name;
    private Type type;
    private String defaultValue;
    private IValidator validator;
    private String help = "";

    /* package */ KeyMeta(String name, Type type, String defaultValue) {
        this.name = name;
        this.type = type;
        this.defaultValue = defaultValue;
    }

    public KeyMeta(CategoryMeta category, String name, Type type, String defaultValue) {
        this.category = category;
        this.name = name;
        this.type = type;
        this.defaultValue = defaultValue;
    }

    public String getName() {
        return name;
    }

    public Type getType() {
        return type;
    }

    public String getDefaultValue() {
        return defaultValue;
    }

    public CategoryMeta getCategory() {
        return category;
    }

    /* package */ void setCategory(CategoryMeta category) {
        if (this.category != null) {
            throw new IllegalArgumentException("Reused KeyMeta " + this + "?");
        }
        this.category = category;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof KeyMeta) {
            KeyMeta other = (KeyMeta) obj;
            return name.equals(other.name) && category.equals(other.category);
        }
        return super.equals(obj);
    }

    @Override
    public int hashCode() {
        return name.hashCode() + category.hashCode();
    }

    public IValidator getValidator() {
        return validator;
    }

    public void setValidator(IValidator validator) {
        this.validator = validator;
    }

    public void setHelp(String help) {
        this.help  = help;
    }

    public String getHelp() {
        return help;
    }

}

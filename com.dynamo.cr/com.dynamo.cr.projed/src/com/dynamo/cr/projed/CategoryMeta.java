package com.dynamo.cr.projed;

import java.util.Collections;
import java.util.List;

public class CategoryMeta {
    private String name;
    private List<KeyMeta> keys;
    private String description;

    public CategoryMeta(String name, String description, List<KeyMeta> keys) {
        this.name = name;
        this.description = description;
        this.keys = keys;

        for (KeyMeta keyMeta : keys) {
            keyMeta.setCategory(this);
        }
    }

    public String getName() {
        return name;
    }

    public String getDescription() {
        return description;
    }

    public List<KeyMeta> getKeys() {
        return Collections.unmodifiableList(keys);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof CategoryMeta) {
            CategoryMeta other = (CategoryMeta) obj;
            return name.equals(other.name);
        }
        return super.equals(obj);
    }

    @Override
    public int hashCode() {
        return name.hashCode();
    }

}


package com.dynamo.cr.tileeditor.core;

public class Tag {
    public static final int TYPE_INFO = 0;
    public static final int TYPE_WARNING = 0;
    public static final int TYPE_ERROR = 0;

    private final String id;
    private final int type;
    private final String message;

    public Tag(String id, int type, String message) {
        this.id = id;
        this.type = type;
        this.message = message;
    }

    public String getId() {
        return this.id;
    }

    public int getType() {
        return this.type;
    }

    public String getMessage() {
        return this.message;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj instanceof Tag) {
            Tag a = (Tag)obj;
            return this.id.equals(a.id);
        } else if (obj instanceof String) {
            return this.id.equals(obj);
        } else {
            return super.equals(obj);
        }
    }
}

package com.dynamo.cr.editor.ui.tip;

public class Tip {

    private String id;
    private String title;
    private String tip;
    private String link;

    public Tip(String id, String title, String tip, String link) {
        this.id = id;
        this.title = title;
        this.tip = tip;
        this.link = link;
    }

    public String getId() {
        return id;
    }

    public String getTitle() {
        return title;
    }

    public String getTip() {
        return tip;
    }

    public String getLink() {
        return link;
    }

}

package com.unnison.ajaxcrawler.meta;

//@javax.annotation.Generated(value = { "slim3-gen", "@VERSION@" }, date = "2015-02-10 12:16:45")
/** */
public final class HostPlaceMeta extends org.slim3.datastore.ModelMeta<com.unnison.ajaxcrawler.model.HostPlace> {

    /** */
    public final org.slim3.datastore.StringUnindexedAttributeMeta<com.unnison.ajaxcrawler.model.HostPlace> html = new org.slim3.datastore.StringUnindexedAttributeMeta<com.unnison.ajaxcrawler.model.HostPlace>(this, "html", "html");

    /** */
    public final org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.HostPlace, com.google.appengine.api.datastore.Key> key = new org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.HostPlace, com.google.appengine.api.datastore.Key>(this, "__key__", "key", com.google.appengine.api.datastore.Key.class);

    /** */
    public final org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.HostPlace, java.util.Date> lastFetch = new org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.HostPlace, java.util.Date>(this, "lastFetch", "lastFetch", java.util.Date.class);

    private static final HostPlaceMeta slim3_singleton = new HostPlaceMeta();

    /**
     * @return the singleton
     */
    public static HostPlaceMeta get() {
       return slim3_singleton;
    }

    /** */
    public HostPlaceMeta() {
        super("HostPlace", com.unnison.ajaxcrawler.model.HostPlace.class);
    }

    @Override
    public com.unnison.ajaxcrawler.model.HostPlace entityToModel(com.google.appengine.api.datastore.Entity entity) {
        com.unnison.ajaxcrawler.model.HostPlace model = new com.unnison.ajaxcrawler.model.HostPlace();
        model.setHtml(textToString((com.google.appengine.api.datastore.Text) entity.getProperty("html")));
        model.setKey(entity.getKey());
        model.setLastFetch((java.util.Date) entity.getProperty("lastFetch"));
        return model;
    }

    @Override
    public com.google.appengine.api.datastore.Entity modelToEntity(java.lang.Object model) {
        com.unnison.ajaxcrawler.model.HostPlace m = (com.unnison.ajaxcrawler.model.HostPlace) model;
        com.google.appengine.api.datastore.Entity entity = null;
        if (m.getKey() != null) {
            entity = new com.google.appengine.api.datastore.Entity(m.getKey());
        } else {
            entity = new com.google.appengine.api.datastore.Entity(kind);
        }
        entity.setUnindexedProperty("html", stringToText(m.getHtml()));
        entity.setProperty("lastFetch", m.getLastFetch());
        return entity;
    }

    @Override
    protected com.google.appengine.api.datastore.Key getKey(Object model) {
        com.unnison.ajaxcrawler.model.HostPlace m = (com.unnison.ajaxcrawler.model.HostPlace) model;
        return m.getKey();
    }

    @Override
    protected void setKey(Object model, com.google.appengine.api.datastore.Key key) {
        validateKey(key);
        com.unnison.ajaxcrawler.model.HostPlace m = (com.unnison.ajaxcrawler.model.HostPlace) model;
        m.setKey(key);
    }

    @Override
    protected long getVersion(Object model) {
        throw new IllegalStateException("The version property of the model(com.unnison.ajaxcrawler.model.HostPlace) is not defined.");
    }

    @Override
    protected void incrementVersion(Object model) {
    }

    @Override
    protected void prePut(Object model) {
        assignKeyIfNecessary(model);
        incrementVersion(model);
    }

    @Override
    public String getSchemaVersionName() {
        return "slim3.schemaVersion";
    }

    @Override
    public String getClassHierarchyListName() {
        return "slim3.classHierarchyList";
    }

}
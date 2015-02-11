package com.unnison.ajaxcrawler.meta;

//@javax.annotation.Generated(value = { "slim3-gen", "@VERSION@" }, date = "2015-02-10 12:16:45")
/** */
public final class HostMeta extends org.slim3.datastore.ModelMeta<com.unnison.ajaxcrawler.model.Host> {

    /** */
    public final org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.Host, java.lang.Integer> fetchRatio = new org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.Host, java.lang.Integer>(this, "fetchRatio", "fetchRatio", int.class);

    /** */
    public final org.slim3.datastore.StringCollectionAttributeMeta<com.unnison.ajaxcrawler.model.Host, java.util.ArrayList<java.lang.String>> indexes = new org.slim3.datastore.StringCollectionAttributeMeta<com.unnison.ajaxcrawler.model.Host, java.util.ArrayList<java.lang.String>>(this, "indexes", "indexes", java.util.ArrayList.class);

    /** */
    public final org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.Host, com.google.appengine.api.datastore.Key> key = new org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.Host, com.google.appengine.api.datastore.Key>(this, "__key__", "key", com.google.appengine.api.datastore.Key.class);

    /** */
    public final org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.Host, java.util.Date> lastFetch = new org.slim3.datastore.CoreAttributeMeta<com.unnison.ajaxcrawler.model.Host, java.util.Date>(this, "lastFetch", "lastFetch", java.util.Date.class);

    private static final HostMeta slim3_singleton = new HostMeta();

    /**
     * @return the singleton
     */
    public static HostMeta get() {
       return slim3_singleton;
    }

    /** */
    public HostMeta() {
        super("Host", com.unnison.ajaxcrawler.model.Host.class);
    }

    @Override
    public com.unnison.ajaxcrawler.model.Host entityToModel(com.google.appengine.api.datastore.Entity entity) {
        com.unnison.ajaxcrawler.model.Host model = new com.unnison.ajaxcrawler.model.Host();
        model.setFetchRatio(longToPrimitiveInt((java.lang.Long) entity.getProperty("fetchRatio")));
        model.setIndexes(toList(java.lang.String.class, entity.getProperty("indexes")));
        model.setKey(entity.getKey());
        model.setLastFetch((java.util.Date) entity.getProperty("lastFetch"));
        return model;
    }

    @Override
    public com.google.appengine.api.datastore.Entity modelToEntity(java.lang.Object model) {
        com.unnison.ajaxcrawler.model.Host m = (com.unnison.ajaxcrawler.model.Host) model;
        com.google.appengine.api.datastore.Entity entity = null;
        if (m.getKey() != null) {
            entity = new com.google.appengine.api.datastore.Entity(m.getKey());
        } else {
            entity = new com.google.appengine.api.datastore.Entity(kind);
        }
        entity.setProperty("fetchRatio", m.getFetchRatio());
        entity.setProperty("indexes", m.getIndexes());
        entity.setProperty("lastFetch", m.getLastFetch());
        return entity;
    }

    @Override
    protected com.google.appengine.api.datastore.Key getKey(Object model) {
        com.unnison.ajaxcrawler.model.Host m = (com.unnison.ajaxcrawler.model.Host) model;
        return m.getKey();
    }

    @Override
    protected void setKey(Object model, com.google.appengine.api.datastore.Key key) {
        validateKey(key);
        com.unnison.ajaxcrawler.model.Host m = (com.unnison.ajaxcrawler.model.Host) model;
        m.setKey(key);
    }

    @Override
    protected long getVersion(Object model) {
        throw new IllegalStateException("The version property of the model(com.unnison.ajaxcrawler.model.Host) is not defined.");
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
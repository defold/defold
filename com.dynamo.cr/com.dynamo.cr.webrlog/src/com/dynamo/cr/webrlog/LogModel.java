package com.dynamo.cr.webrlog;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import org.joda.time.DateTime;
import org.joda.time.format.DateTimeFormatter;
import org.joda.time.format.DateTimeFormatterBuilder;
import org.joda.time.format.ISODateTimeFormat;

import com.dynamo.cr.rlog.proto.RLog.Issue;
import com.dynamo.cr.rlog.proto.RLog.Record;
import com.dynamo.cr.rlog.proto.RLog.Severity;
import com.dynamo.cr.rlog.proto.RLog.StackTrace;
import com.google.appengine.api.datastore.Blob;
import com.google.appengine.api.datastore.DatastoreService;
import com.google.appengine.api.datastore.DatastoreServiceFactory;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.EntityNotFoundException;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.datastore.Query;
import com.google.appengine.api.datastore.Query.CompositeFilterOperator;
import com.google.appengine.api.datastore.Query.Filter;
import com.google.appengine.api.datastore.Query.FilterOperator;
import com.google.appengine.api.datastore.Query.SortDirection;
import com.google.protobuf.InvalidProtocolBufferException;

public class LogModel {
    public static DateTimeFormatter isoFormat = ISODateTimeFormat.dateTime();
    public static DateTimeFormatter monthFormat = new DateTimeFormatterBuilder().appendYear(4, 4).appendLiteral("-").appendMonthOfYear(2).toFormatter();

    public static String sha1(String str) {
        MessageDigest sha1;
        try {
            sha1 = MessageDigest.getInstance("SHA1");
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException(e);
        }

        sha1.update(str.getBytes());
        byte[] digest = sha1.digest();
        StringBuilder sb = new StringBuilder();
        for (byte b : digest) {
            sb.append(Integer.toHexString(0xff & b));
        }

        return sb.toString();
    }

    private static void toEntity(Record record, Entity entity) {
        DateTime date = isoFormat.parseDateTime(record.getDate());

        entity.setProperty("ip", record.getIp());
        entity.setProperty("date", date.toDate());
        entity.setProperty("message", record.getMessage());
        entity.setProperty("severity", record.getSeverity().getNumber());
        entity.setProperty("plugin", record.getPlugin());
        entity.setProperty("version", record.getVersion());
        if (record.hasStackTrace()) {
            // If stack-trace is available the sha1 is based on the stack-trace
            entity.setProperty("stack_trace", new Blob(record.getStackTrace().toByteArray()));
            entity.setProperty("sha1", sha1(record.getStackTrace().toString()));
        } else {
            // Otherwise message, severity, plugin and version
            StringBuilder sb = new StringBuilder();
            sb.append(record.getMessage());
            sb.append(record.getSeverity().toString());
            sb.append(record.getPlugin());
            sb.append(record.getVersion());
            entity.setProperty("sha1", sha1(sb.toString()));
        }
    }

    private static Record toRecord(Entity entity) throws InvalidProtocolBufferException {
        Blob stackTraceData = (Blob) entity.getProperty("stack_trace");
        long severity = (Long) entity.getProperty("severity");
        Record.Builder recordBuilder = Record.newBuilder()
                .setDate(isoFormat.print(new DateTime((Date) entity.getProperty("date"))))
                .setIp((String) entity.getProperty("ip"))
                .setMessage((String) entity.getProperty("message"))
                .setPlugin((String) entity.getProperty("plugin"))
                .setVersion((String) entity.getProperty("version"))
                .setSeverity(Severity.valueOf((int) severity));

        if (entity.hasProperty("stack_trace")) {
            StackTrace.Builder st = StackTrace.newBuilder().mergeFrom(stackTraceData.getBytes());
            recordBuilder.setStackTrace(st);
        }
        return recordBuilder.build();
    }

    public static Record getRecord(Key key) {
        DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();

        Entity entity;
        try {
            entity = datastore.get(key);
            return toRecord(entity);
        } catch (EntityNotFoundException e) {
            return null;
        } catch (InvalidProtocolBufferException e) {
            throw new RuntimeException(e);
        }
    }

    public static void putRecord(Record record) {
        DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();
        Key parent = monthKey(isoFormat.parseDateTime(record.getDate()));

        Entity month;
        try {
            month = datastore.get(parent);
        } catch (EntityNotFoundException e) {
            month = new Entity(parent);
        }
        month.setProperty("processed", new Date(0));
        datastore.put(month);

        Entity entity = new Entity("record", parent);
        toEntity(record, entity);
        datastore.put(entity);
    }

    private static Issue toIssue(Entity entity) throws InvalidProtocolBufferException {
        Record record = toRecord(entity);
        Issue issue = Issue.newBuilder()
                .setFixed((Boolean) entity.getProperty("fixed"))
                .setRecord(record)
                .setSha1((String) entity.getProperty("sha1"))
                .setReported(((Long) entity.getProperty("reported")).intValue())
                .build();
        return issue;
    }

    public static Key monthKey(DateTime date) {
        String dateKey = monthFormat.print(date);
        Key parent = KeyFactory.createKey("month", dateKey);
        return parent;
    }

    public static Key issueKey(DateTime date, String sha1) {
        Key monthKey = KeyFactory.createKey("month", monthFormat.print(date));
        return KeyFactory.createKey(monthKey, "issue", sha1);
    }

    public static Issue getIssue(Key key) {
        DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();
        Entity entity;
        try {
            entity = datastore.get(key);
            Issue issue = toIssue(entity);
            return issue;
        } catch (EntityNotFoundException e) {
            return null;
        } catch (InvalidProtocolBufferException e) {
            throw new RuntimeException(e);
        }
    }

    public static List<Issue> getMonthIssues(DateTime date) {
        DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();
        Key monthKey = monthKey(date);
        ArrayList<Issue> lst = new ArrayList<Issue>();
        Iterable<Entity> i = datastore.prepare(new Query("issue", monthKey )).asIterable();
        for (Entity entity : i) {
            try {
                lst.add(toIssue(entity));
            } catch (InvalidProtocolBufferException e) {
                throw new RuntimeException(e);
            }
        }
        return lst;
    }

    public static List<Issue> getActiveIssues(int maxAgeDags) {
        DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();
        ArrayList<Issue> lst = new ArrayList<Issue>();
        Date date = new DateTime().minusDays(maxAgeDags).toDate();
        Filter filter = CompositeFilterOperator.and(FilterOperator.EQUAL.of("fixed", false),
                                                    FilterOperator.GREATER_THAN_OR_EQUAL.of("date", date));

        Query q = new Query("issue")
            .setFilter(filter)
            .addSort("date", SortDirection.DESCENDING);
        Iterable<Entity> i = datastore.prepare(q).asIterable();
        for (Entity entity : i) {
            try {
                lst.add(toIssue(entity));
            } catch (InvalidProtocolBufferException e) {
                throw new RuntimeException(e);
            }
        }
        return lst;
    }

    public static void putIssue(Key key, Issue issue) {
        DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();
        Entity entity = new Entity(key);
        toEntity(issue.getRecord(), entity);
        entity.setProperty("fixed", issue.getFixed());
        entity.setProperty("reported", issue.getReported());
        entity.setProperty("sha1", key.getName());
        datastore.put(entity);
    }

}

package com.dynamo.cr.webrlog.test;

import static com.google.appengine.api.datastore.FetchOptions.Builder.withLimit;
import static org.junit.Assert.assertEquals;

import java.util.Date;
import java.util.List;

import org.joda.time.DateTime;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.rlog.proto.RLog.Issue;
import com.dynamo.cr.rlog.proto.RLog.Record;
import com.dynamo.cr.rlog.proto.RLog.Severity;
import com.dynamo.cr.rlog.proto.RLog.StackTrace;
import com.dynamo.cr.rlog.proto.RLog.StackTrace.Builder;
import com.dynamo.cr.rlog.proto.RLog.StackTraceElement;
import com.dynamo.cr.webrlog.LogModel;
import com.dynamo.cr.webrlog.tasks.ProcessTask;
import com.google.appengine.api.datastore.DatastoreService;
import com.google.appengine.api.datastore.DatastoreServiceFactory;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.datastore.Query;
import com.google.appengine.tools.development.testing.LocalDatastoreServiceTestConfig;
import com.google.appengine.tools.development.testing.LocalServiceTestHelper;

public class RLogTest {

    private final LocalServiceTestHelper helper = new LocalServiceTestHelper(
            new LocalDatastoreServiceTestConfig());
    private DatastoreService ds;

    @Before
    public void setUp() {
        ds = DatastoreServiceFactory.getDatastoreService();
        helper.setUp();
    }

    @After
    public void tearDown() {
        helper.tearDown();
    }

    Record createRecord(String date, StackTraceElement... stackTrace) {

        Record.Builder b = Record.newBuilder()
                .setDate(date)
                .setIp("127.0.0.1")
                .setMessage("a message")
                .setPlugin("plugin")
                .setVersion("1.0")
                .setSeverity(Severity.WARNING);

        Builder stb = StackTrace.newBuilder();
        for (StackTraceElement stackTraceElement : stackTrace) {
            stb.addElement(stackTraceElement);
        }
        stb.setMessage("exception message");
        b.setStackTrace(stb);

        return b.build();
    }

    Record createRecord(String date) {
        return createRecord(date, new StackTraceElement[0]);
    }

    StackTraceElement createStackTraceElement(String method, int line) {
        return StackTraceElement.newBuilder()
            .setFile("Test.java")
            .setJavaClass("Test")
            .setLine(line)
            .setMethod(method)
            .build();
    }

    int recordCount() {
        return ds.prepare(new Query("record")).countEntities(withLimit(1000));
    }

    int recordCount(String month) {
        Key key = KeyFactory.createKey("month", month);
        return ds.prepare(new Query("record", key)).countEntities(withLimit(1000));
    }

    int issueCount() {
        return ds.prepare(new Query("issue")).countEntities(withLimit(1000));
    }

    int issueCount(DateTime date) {
        return LogModel.getMonthIssues(date).size();
    }

    String toIso(DateTime dateTime) {
        return LogModel.isoFormat.print(dateTime);
    }

    String toMonth(DateTime dateTime) {
        return LogModel.monthFormat.print(dateTime);
    }

    @Test
    public void testLogRecord() {
        assertEquals(0, ds.prepare(new Query("record")).countEntities(withLimit(10)));

        String date = "2012-08-09T12:56:32.124+02:00";
        for (int i = 0; i < 2; ++i) {
            LogModel.putRecord(createRecord(date));
        }
        assertEquals(2, recordCount());
        assertEquals(2, recordCount("2012-08"));
        assertEquals(0, recordCount("2012-01"));
    }

    @Test
    public void testProcess() throws Exception {
        DateTime now = new DateTime();
        Record record = createRecord(toIso(now), createStackTraceElement("m1", 1), createStackTraceElement("m1", 2));
        for (int i = 0; i < 2; ++i) {
            LogModel.putRecord(record);
        }

        assertEquals(0, issueCount(now));

        int monthsProcessed = ProcessTask.process();
        assertEquals(1, monthsProcessed);
        assertEquals(1, issueCount(now));
        assertEquals(1, issueCount());

        monthsProcessed = ProcessTask.process();
        assertEquals(0, monthsProcessed);
    }

    void markFixed(Issue issue) {
        DateTime date = new DateTime(issue.getRecord().getDate());
        Issue issuePrim = Issue.newBuilder().mergeFrom(issue).setFixed(true).build();
        LogModel.putIssue(LogModel.issueKey(date, issue.getSha1()), issuePrim);
    }

    @Test
    public void testIssueFixed() throws Exception {
        DateTime now = new DateTime();
        Record record = createRecord(toIso(now), createStackTraceElement("m1", 1), createStackTraceElement("m1", 2));
        for (int i = 0; i < 2; ++i) {
            LogModel.putRecord(record);
        }

        assertEquals(0, issueCount(now));

        int monthsProcessed = ProcessTask.process();
        assertEquals(1, monthsProcessed);
        Issue issue = LogModel.getMonthIssues(now).get(0);
        assertEquals(false, issue.getFixed());
        markFixed(issue);

        // Reset "processed" attribute to force re-processing of month
        Entity month = ds.get(KeyFactory.createKey("month", toMonth(now)));
        month.setProperty("processed", new Date(0));
        ds.put(month);

        monthsProcessed = ProcessTask.process();
        assertEquals(1, monthsProcessed);

        issue = LogModel.getMonthIssues(now).get(0);
        // Assert that the issue still is marked as fixed
        assertEquals(true, issue.getFixed());

        // Add a new record
        LogModel.putRecord(record);
        monthsProcessed = ProcessTask.process();
        assertEquals(1, monthsProcessed);

        issue = LogModel.getMonthIssues(now).get(0);
        // Assert that the issue fixed attribute is reset
        assertEquals(true, !issue.getFixed());
    }

    @Test
    public void testProcessTwoMonths() throws Exception {
        DateTime date1 = new DateTime();
        DateTime date2 = date1.plusMonths(1);

        // Create equivalent records for different months
        Record r1 = createRecord(toIso(date1), createStackTraceElement("m1", 1), createStackTraceElement("m1", 2));
        LogModel.putRecord(r1);
        LogModel.putRecord(r1);
        Record r2 = createRecord(toIso(date2), createStackTraceElement("m1", 1), createStackTraceElement("m1", 2));
        LogModel.putRecord(r2);
        LogModel.putRecord(r2);
        LogModel.putRecord(r2);

        // Assert that we have one issue per month and two in total
        assertEquals(0, issueCount());
        int monthsProcessed = ProcessTask.process();
        assertEquals(2, monthsProcessed);
        assertEquals(1, issueCount(date1));
        assertEquals(1, issueCount(date2));
        assertEquals(2, issueCount());

        List<Issue> issues1 = LogModel.getMonthIssues(date1);
        assertEquals(1, issues1.size());
        Issue issue1 = issues1.get(0);
        assertEquals(2, issue1.getReported());

        List<Issue> issues2 = LogModel.getMonthIssues(date2);
        List<Issue> issue2 = issues2;
        assertEquals(1, issue2.size());
        assertEquals(3, issue2.get(0).getReported());

        // Sha1 must be equal
        assertEquals(issue1.getSha1(), issue2.get(0).getSha1());

        int maxDays = 1000;
        // Mark issues as fixed and validte
        assertEquals(2, LogModel.getActiveIssues(maxDays).size());
        markFixed(issue1);
        assertEquals(1, LogModel.getActiveIssues(maxDays).size());
        markFixed(issue2.get(0));
        // All fixed and active count is now zero
        assertEquals(0, LogModel.getActiveIssues(maxDays).size());
        // All fixed but the total count is still two
        assertEquals(2, issueCount());
    }

}

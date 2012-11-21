package com.dynamo.cr.webrlog.tasks;

import static com.google.appengine.api.taskqueue.TaskOptions.Builder.withUrl;

import java.io.IOException;
import java.util.Date;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.logging.Logger;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.dynamo.cr.rlog.proto.RLog.Issue;
import com.dynamo.cr.rlog.proto.RLog.Record;
import com.dynamo.cr.webrlog.LogModel;
import com.google.appengine.api.datastore.DatastoreService;
import com.google.appengine.api.datastore.DatastoreServiceFactory;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.EntityNotFoundException;
import com.google.appengine.api.datastore.FetchOptions;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.datastore.Query;
import com.google.appengine.api.datastore.Query.FilterPredicate;
import com.google.appengine.api.taskqueue.Queue;
import com.google.appengine.api.taskqueue.QueueFactory;
import com.google.appengine.api.taskqueue.TaskOptions.Method;

public class ProcessTask extends HttpServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger log = Logger.getLogger(ProcessTask.class.getName());


    public static int process() {
        DatastoreService ds = DatastoreServiceFactory.getDatastoreService();

        Iterable<Entity> records = ds.prepare(new Query("month")).asIterable();
        Set<Key> months = new HashSet<Key>();
        for (Entity entity : records) {
            Date processed = (Date) entity.getProperty("processed");
            Query notProcessed = new Query("record", entity.getKey());
            notProcessed.setFilter(new FilterPredicate("date", Query.FilterOperator.GREATER_THAN, processed));
            int count = ds.prepare(notProcessed).countEntities(FetchOptions.Builder.withDefaults());

            if (count > 0) {
                months.add(entity.getKey());
            }
        }

        for (Key month : months) {
            processMonth(ds, month);
        }

        return months.size();
    }

    private static void processMonth(DatastoreService ds, Key monthKey) {
        try {
            log.info("Processing " + monthKey);
            Entity month = ds.get(monthKey);

            boolean gotData = true;
            int offset = 0;
            int limit = 100;

            Map<String, Issue.Builder> issues = new HashMap<String, Issue.Builder>();
            while (gotData) {
                List<Entity> recordEntities = ds.prepare(new Query("record", monthKey)).asList(FetchOptions.Builder.withOffset(offset).limit(limit));
                gotData = recordEntities.size() > 0;
                log.info("Month batch: " + recordEntities.size());
                for (Entity entity : recordEntities) {
                    Record record = LogModel.getRecord(entity.getKey());
                    String sha1 = (String) entity.getProperty("sha1");

                    Issue.Builder b = issues.get(sha1);
                    if (b == null) {
                        b = Issue.newBuilder()
                                .setSha1(sha1)
                                .setFixed(false)
                                .setRecord(record)
                                .setReported(0);
                        issues.put(sha1, b);
                    }
                    b.setReported(b.getReported() + 1);
                }
                offset += recordEntities.size();
            }

            for (Issue.Builder i : issues.values()) {
                Key issueKey = KeyFactory.createKey(monthKey, "issue", i.getSha1());

                Issue prevIssue = LogModel.getIssue(issueKey);
                if (prevIssue != null && prevIssue.getReported() == i.getReported()) {
                    // Preserve only "fixed" when reported-count is unchanged
                    i.setFixed(prevIssue.getFixed());
                }

                LogModel.putIssue(issueKey, i.build());
            }

            month.setProperty("processed", new Date());
            ds.put(month);
        } catch (EntityNotFoundException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {

        process();
        resp.setStatus(HttpServletResponse.SC_OK);
        resp.setContentType("text/plain");
        resp.getWriter().println("Process task completed");
    }

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {

        Queue queue = QueueFactory.getDefaultQueue();
        queue.add(withUrl("/tasks/process").method(Method.POST));

        resp.setStatus(HttpServletResponse.SC_OK);
        resp.setContentType("text/plain");
        resp.getWriter().println("Process task enqueued");
    }
}


package com.dynamo.cr.web2.server;

import static com.google.appengine.api.taskqueue.TaskOptions.Builder.withUrl;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Date;
import java.util.HashSet;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.servlet.ServletContext;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.google.appengine.api.search.Document;
import com.google.appengine.api.search.Field;
import com.google.appengine.api.search.GetRequest;
import com.google.appengine.api.search.GetResponse;
import com.google.appengine.api.search.Index;
import com.google.appengine.api.search.IndexSpec;
import com.google.appengine.api.search.SearchServiceFactory;
import com.google.appengine.api.taskqueue.Queue;
import com.google.appengine.api.taskqueue.QueueFactory;
import com.google.appengine.api.taskqueue.TaskOptions.Method;

public class SearchIndexServlet extends HttpServlet {

    private static final long serialVersionUID = 1L;

    private static final Logger logger = Logger.getLogger(SearchIndexServlet.class
            .getName());

    private static final Index searchIndex = SearchServiceFactory.getSearchService()
            .getIndex(IndexSpec.newBuilder().setName("shared_index"));

    private String loadDocument(String path) throws IOException {
        ServletContext context = getServletContext();
        InputStream stream = context.getResourceAsStream(path);

        ByteArrayOutputStream output = new ByteArrayOutputStream(1024 * 128);
        byte[] buf = new byte[1024 * 128];
        int n = stream.read(buf);
        while (n > 0) {
            output.write(buf, 0, n);
            n = stream.read(buf);
        }
        stream.close();
        output.close();
        return new String(output.toByteArray(), "UTF-8");
    }

    private void createIndex(HttpServletRequest req) throws IOException {
        Pattern titlePattern = Pattern.compile("<title.*?>(.*?)</title>");
        String indexMapString = loadDocument("/index_map.txt");
        Set<String> toIndex = new HashSet<String>(Arrays.asList(indexMapString.split("\n")));

        // Clear stale documents
        GetRequest request = GetRequest.newBuilder().setReturningIdsOnly(true).build();
        GetResponse<Document> response = searchIndex.getRange(request);

        Set<String> currentIndex = new HashSet<String>();
        for (Document document : response) {
            currentIndex.add(document.getId());
        }
        Set<String> toRemove = new HashSet<String>();
        toRemove.addAll(currentIndex);
        toRemove.removeAll(toIndex);
        searchIndex.delete(toRemove);

        // Actual indexing
        for (String docPath : toIndex) {
            String docHtml = loadDocument(docPath);

            String title = "unnamed";
            Matcher matcher = titlePattern.matcher(docHtml);
            if (matcher.find()) {
                title = matcher.group(1);
            } else {
                logger.log(Level.SEVERE, String.format("No title found for document: %s", docPath));
            }

            Document.Builder docBuilder = Document
                    .newBuilder()
                    .setId(docPath)
                    .addField(
                            Field.newBuilder().setName("content")
                                    .setHTML(docHtml))
                    .addField(
                            Field.newBuilder().setName("title")
                                    .setText(title))
                    .addField(
                            Field.newBuilder().setName("published")
                                    .setDate(new Date()));

            Document doc = docBuilder.build();
            logger.info("Adding document: " + docPath);
            try {
                searchIndex.put(doc);
            } catch (RuntimeException e) {
                logger.log(Level.SEVERE, "Failed to add " + doc, e);
            }
        }
    }

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {
        createIndex(req);
        resp.setStatus(200);
        resp.setContentType("text/plain");
        resp.getWriter().println("Index updated");
    }

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {
        Queue queue = QueueFactory.getDefaultQueue();
        queue.add(withUrl("/search-index").method(Method.POST));
        resp.setStatus(200);
        resp.setContentType("text/plain");
        resp.getWriter().println("Index task queued");
    }
}


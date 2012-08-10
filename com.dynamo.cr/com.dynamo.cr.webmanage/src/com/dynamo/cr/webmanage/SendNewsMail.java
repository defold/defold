package com.dynamo.cr.webmanage;

import static com.google.appengine.api.taskqueue.TaskOptions.Builder.withUrl;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.google.appengine.api.taskqueue.Queue;
import com.google.appengine.api.taskqueue.QueueFactory;
import com.google.appengine.api.taskqueue.TaskOptions.Method;

public class SendNewsMail extends HttpServlet {

    private static final long serialVersionUID = 1L;
    private static final Logger logger = Logger.getLogger(SendNewsMail.class.getName());
    public static final int BATCH_SIZE = 10;

    private void createTask(String subject, List<Recipient> recipients, String from, String name, String message, String serverUrl) throws IOException {
        Queue queue = QueueFactory.getDefaultQueue();
        SendMailTaskParams params = new SendMailTaskParams(subject, from, name, message, recipients, serverUrl);
        byte[] paramsData = params.serialize();

        queue.add(withUrl("/send-news-mail-task")
                .payload(paramsData)
                .method(Method.POST));
    }

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {
        String subject = req.getParameter("subject");
        String recipientsText = req.getParameter("recipients");
        String from = req.getParameter("from");
        String name = req.getParameter("name");
        String message = req.getParameter("message");
        String serverUrl = req.getParameter("serverUrl");

        List<Recipient> recipients = new ArrayList<Recipient>();
        for (String line : recipientsText.split("\n")) {
            line = line.trim();
            if (line.length() > 0) {
                String[] emailKey = line.split(",");
                if (emailKey.length != 2) {
                    logger.log(Level.SEVERE, String.format("Invalid recipients line: %s", line));
                } else {
                    Recipient recipient = new Recipient(emailKey[0].trim(), emailKey[1].trim());
                    recipients.add(recipient);
                }
            }
        }

        while (recipients.size() > 0) {
            List<Recipient> batch = new ArrayList<Recipient>();
            for (int i = 0; i < BATCH_SIZE && recipients.size() > 0; ++i) {
                batch.add(recipients.remove(0));
            }
            createTask(subject, batch, from, name, message, serverUrl);
        }

        resp.setStatus(200);
        resp.setContentType("text/plain");
        resp.getWriter().println("Mail tasks created");
    }
}

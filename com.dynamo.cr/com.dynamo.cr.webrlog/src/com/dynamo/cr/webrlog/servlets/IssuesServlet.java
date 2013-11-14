package com.dynamo.cr.webrlog.servlets;

import java.io.IOException;
import java.io.StringWriter;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import org.joda.time.DateTime;

import com.dynamo.cr.rlog.proto.RLog.Issue;
import com.dynamo.cr.rlog.proto.RLog.IssueList;
import com.dynamo.cr.webrlog.LogModel;
import com.fasterxml.jackson.core.JsonFactory;
import com.fasterxml.jackson.core.JsonGenerationException;
import com.fasterxml.jackson.core.JsonGenerator;
import com.google.appengine.api.datastore.Key;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.EnumValueDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Message;

public class IssuesServlet extends HttpServlet {

    private static final long serialVersionUID = 1L;

    void ObjectToJSON(Object o, JsonGenerator generator) throws JsonGenerationException, IOException {
        if (o instanceof Integer) {
            generator.writeNumber((Integer) o);
        }
        else if (o instanceof Long) {
            generator.writeNumber((Long) o);
        }
        else if (o instanceof Number) {
            generator.writeNumber((Double) o);
        }
        else if (o instanceof String) {
            generator.writeString((String) o);
        }
        else if (o instanceof Boolean) {
            generator.writeBoolean((Boolean) o);
        }
        else if (o instanceof EnumValueDescriptor) {
            generator.writeString(((EnumValueDescriptor) o).getName());
        }
        else if (o instanceof Message) {
            MessageToJSON((Message) o, generator);
        }
        else {
            throw new RuntimeException(String.format("Unsupported type: %s", o));
        }
    }

    void MessageToJSON(Message m, JsonGenerator generator) throws JsonGenerationException, IOException {
        generator.writeStartObject();
        Descriptor desc = m.getDescriptorForType();
        List<FieldDescriptor> fields = desc.getFields();
        for (FieldDescriptor fieldDescriptor : fields) {

            if (fieldDescriptor.isRepeated()) {
                @SuppressWarnings("unchecked")
                List<Object> list = (List<Object>) m.getField(fieldDescriptor);
                generator.writeArrayFieldStart(fieldDescriptor.getName());
                for (Object object : list) {
                    ObjectToJSON(object, generator);
                }
                generator.writeEndArray();
            }
            else {
                generator.writeFieldName(fieldDescriptor.getName());
                ObjectToJSON(m.getField(fieldDescriptor), generator);
            }
        }
        generator.writeEndObject();
    }

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {

        List<Issue> issues = LogModel.getActiveIssues(30);
        IssueList issueList = IssueList.newBuilder().addAllIssue(issues).build();

        StringWriter writer = new StringWriter();
        JsonGenerator generator = (new JsonFactory()).createJsonGenerator(writer);
        MessageToJSON(issueList, generator);
        generator.close();

        resp.setStatus(HttpServletResponse.SC_OK);
        resp.setContentType("application/json");
        resp.getWriter().println(writer.toString());
    }

    @Override
    protected void doPut(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {

        Pattern pattern = Pattern.compile("/issues/(.+?)/(.+?)/(.+?)");
        String uri = req.getRequestURI();
        Matcher matcher = pattern.matcher(uri);
        if (matcher.matches()) {
            String dateText = matcher.group(1);
            String sha1 = matcher.group(2);
            String trueFalse = matcher.group(3);

            DateTime date = LogModel.isoFormat.parseDateTime(dateText);
            Key issueKey = LogModel.issueKey(date, sha1);
            Issue issue = LogModel.getIssue(issueKey);
            Issue.Builder issueBuilder = Issue.newBuilder(issue);
            if (trueFalse.equals("true")) {
                issueBuilder.setFixed(true);
            } else {
                issueBuilder.setFixed(false);
            }
            LogModel.putIssue(issueKey, issueBuilder.build());

            resp.setStatus(HttpServletResponse.SC_OK);
            resp.setContentType("application/text");
            resp.getWriter().println("Issue updated");
            return;
        }

        resp.setStatus(HttpServletResponse.SC_BAD_REQUEST);
        resp.setContentType("application/text");
        resp.getWriter().println("Invalid request");
    }

}

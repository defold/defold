package com.dynamo.cr.webrlog.servlets;

import java.io.IOException;

import javax.servlet.ServletException;
import javax.servlet.ServletInputStream;
import javax.servlet.http.HttpServlet;
import javax.servlet.http.HttpServletRequest;
import javax.servlet.http.HttpServletResponse;

import com.dynamo.cr.rlog.proto.RLog.Record;
import com.dynamo.cr.webrlog.LogModel;

public class PostRecordServlet extends HttpServlet {

    private static final long serialVersionUID = 1L;

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp)
            throws ServletException, IOException {
        ServletInputStream is = req.getInputStream();
        try {
            Record record = Record.newBuilder().mergeFrom(is).build();
            LogModel.putRecord(record);
            resp.setStatus(HttpServletResponse.SC_OK);
            resp.getOutputStream().print("Log record added");
        } finally {
            is.close();
        }
    }
}

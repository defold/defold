{% extends "base.html" %}
{% block titke %}Search Result{% endblock %}
{% block content %}

<%@ page contentType="text/html;charset=UTF-8" language="java" %>
<%@ page import="com.google.appengine.api.search.*" %>
<%@ page import="com.dynamo.cr.web2.server.Search" %>
<%@ page import="com.dynamo.cr.web2.server.SearchResult" %>
<%@ page import="java.util.List" %>

<div class="container">
    <div class="row">
        <div class="span12">
            <div class="page-header">
                <h1>Search Result</h1>
            </div>
        </div>

        <div class="span6">
            <form class="form-search">
                <input type="text" name="q" class="input-xlarge search-query" value="<%= request.getParameter("q") %>">
                <button type="submit" class="btn btn-primary">Search</button>
            </form>
        </div>

        <div class="span12">
            <%
                String query = request.getParameter("q");
                if (query == null)
                    query = "";

                List<SearchResult> results = Search.search(query);

                for (SearchResult r : results) {
                %>
                    <div class="search-result">
                        <h2>
                        <a href="<%=r.getUrl()%>">
                            <%=r.getTitle()%>
                        </a>
                        </h2>
                        <h3>
                            <%=r.getUrl()%>
                        </h3>
                        <p>
                            <%=r.getContent()%>
                        </p>
                        <small>
                            <%=r.getPublished()%>
                        </small>
                    </div>
                <%
                }
            %>
        </div>
    </div>
</div>


{% endblock %}

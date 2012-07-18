package com.dynamo.cr.web2.server;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;

import com.google.appengine.api.search.Field;
import com.google.appengine.api.search.Index;
import com.google.appengine.api.search.IndexSpec;
import com.google.appengine.api.search.Query;
import com.google.appengine.api.search.QueryOptions;
import com.google.appengine.api.search.Results;
import com.google.appengine.api.search.ScoredDocument;
import com.google.appengine.api.search.SearchServiceFactory;
import com.google.appengine.api.utils.SystemProperty;
import com.google.appengine.api.utils.SystemProperty.Environment;

public class Search {

    private static final Index searchIndex = SearchServiceFactory
            .getSearchService().getIndex(
                    IndexSpec.newBuilder().setName("shared_index"));

    public static List<SearchResult> search(String queryStr) {
        if (queryStr == null) {
            queryStr = "";
        }

        int limit = 25;

        QueryOptions.Builder queryOptionBuilder = QueryOptions.newBuilder()
                .setLimit(limit);

        if (SystemProperty.environment.value() == Environment.Value.Production) {
            queryOptionBuilder.setFieldsToSnippet("content");
        }

        Query query = Query.newBuilder().setOptions(queryOptionBuilder.build())
                .build(queryStr);

        Results<ScoredDocument> searchResult = searchIndex.search(query);
        List<SearchResult> result = new ArrayList<SearchResult>();
        for (ScoredDocument d : searchResult) {
            List<Field> expressions = d.getExpressions();
            String content = null;
            if (expressions != null) {
                for (Field field : expressions) {
                    if ("content".equals(field.getName())) {
                        content = field.getHTML();
                        break;
                    }
                }
            }

            if (content == null) {
                content = "No snippet support on dev-server :-/";
            }


            String title = d.getOnlyField("title").getText();
            Date published = d.getOnlyField("published").getDate();

            // .../sindex.html -> .../
            // .../index.html -> .../
            // NOTE: The order is important, sindex before index
            String url = d.getId().replace("/sindex.html", "");
            url = url.replace("/index.html", "");
            // NOTE: We assume that we can prettify the url be removing .html
            url = url.replace(".html", "");

            // We remove last slash but there's an edge-edge for for root-page
            if (url.equals("")) {
                url = "/";
            }

            result.add(new SearchResult(d.getId(), url, title, published, content));
        }
        return result;
    }
}

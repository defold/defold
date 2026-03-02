;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.web-server
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.lsp.async :as lsp.async]
            [editor.util :as util]
            [reitit.core :as reitit]
            [util.coll :as coll]
            [util.http-server :as http-server])
  (:import [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defn- openapi-paths [router]
  (coll/into-> (reitit/routes router) {}
    (keep
      (fn [[path method->handler]]
        (let [method->operation (coll/into-> method->handler {}
                                  (keep (fn [[method handler]]
                                          (when-let [operation (:openapi (meta handler))]
                                            (coll/pair (util/lower-case* method) operation)))))]
          (when-not (coll/empty? method->operation)
            ;; Convert reitit rest-parameter syntax `{*param}` to OpenAPI `{param}`.
            (coll/pair (string/replace path #"\{\*([^}]+)\}" "{$1}") method->operation)))))))

(defn- openapi-response [request]
  (http-server/json-response
    {:openapi "3.0.3"
     :info {:title "Defold Editor HTTP API"
            :version "1.0"}
     :paths (openapi-paths (:router request))}))

(defn- index-response [project]
  (lsp.async/with-auto-evaluation-context evaluation-context
    (let [project-path (-> project
                           (g/node-value :workspace evaluation-context)
                           (g/node-value :root evaluation-context))
          title (-> project
                    (g/maybe-node-value :settings evaluation-context)
                    (get ["project" "title"]))
          project-title (or title (FilenameUtils/getName project-path))]
      (http-server/response
        200
        {"content-type" "text/html; charset=utf-8"}
        (str "<!doctype html>\n"
             "<html>\n"
             "  <head>\n"
             "    <meta charset=\"utf-8\">\n"
             "    <title>Defold Editor HTTP Server - " project-title "</title>\n"
             "  </head>\n"
             "  <body>\n"
             "    <h1>Defold Editor HTTP Server - " project-title "</h1>\n"
             "    <p><strong>Project:</strong> <code>" project-path "</code></p>\n"
             "    <p><a href=\"/openapi.json\">OpenAPI spec</a></p>\n"
             "  </body>\n"
             "</html>\n")))))

(defn built-in-routes [project]
  {"/" {"GET" (bound-fn [_] (index-response project))}
   "/openapi.json" {"GET" openapi-response}})

(defn make-dynamic-handler
  "Create an HTTP request handler that supports setting dynamic routes

  See [[set-dynamic-routes!]]"
  [built-in-routes]
  (let [current-handler (atom (http-server/router-handler built-in-routes))]
    ^{::built-in-routes built-in-routes
      ::state current-handler}
    (fn handle-request [request]
      (@current-handler request))))

(defn set-dynamic-routes!
  "Set dynamic routes on the HTTP request handler

  If the new route set causes a conflict, throws an exception where ex-data is
  a map with the following keys:
    :type    :path-conflicts
    :data    a map from path+route-data to a set of conflicting path+route-data"
  [web-handler dynamic-routes]
  {:pre [(contains? (meta web-handler) ::state)]}
  (let [{::keys [built-in-routes state]} (meta web-handler)]
    (reset! state (http-server/router-handler (into built-in-routes dynamic-routes))))
  nil)

(comment

  ;; If you are adding built-in OpenAPI routes, this is a handy validator tool
  ;; to run against a running server:
  (println
    (editor.process/exec!
      {:err :stdout}
      "sh" "-lc"
      (str "curl -sS " (http-server/local-url (dev/web-server)) "/openapi.json"
           " | npx -y @redocly/cli lint /dev/stdin"
           "   --skip-rule no-empty-servers"
           "   --skip-rule security-defined"
           "   --skip-rule info-license"
           "   --skip-rule operation-operationId"
           "   --skip-rule operation-4xx-response"
           " || true")))

  #__)

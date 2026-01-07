;; Copyright 2020-2025 The Defold Foundation
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

(ns integration.web-server-test
  (:require [cljfx.api :as fx]
            [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.view :as view]
            [editor.command-requests :as command-requests]
            [editor.console :as console]
            [editor.engine-profiler :as engine-profiler]
            [editor.fs :as fs]
            [editor.handler :as handler]
            [editor.hot-reload :as hot-reload]
            [editor.http-server.prefs :as http-server.prefs]
            [editor.pipeline.bob :as bob]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.ui :as ui]
            [editor.web-server :as web-server]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.http-client :as http]
            [util.http-server :as http-server])
  (:import [java.io ByteArrayInputStream ByteArrayOutputStream]
           [java.nio.charset StandardCharsets]
           [javafx.scene Scene]
           [javafx.scene.layout Region]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defn count-bytes [response]
  (if (is (instance? byte/1 (:body response)))
    (update response :body count)
    response))

(defn ignore-date [response]
  (if (is (contains? (:headers response) "date"))
    (update response :headers dissoc "date")
    response))

(deftest response-construction-test
  (is (= {:status 200} (http-server/response 200)))
  ;; text
  (is (= {:status 200
          :headers {"content-type" "text/plain; charset=utf-8"
                    "content-length" "12"}
          :body 12}
         (count-bytes (http-server/response 200 "Hello world!"))))
  (is (= {:status 200
          :headers {"content-type" "application/octet-stream"
                    "content-length" "6"}
          :body 6}
         (count-bytes (http-server/response 200 (.getBytes "Binary" StandardCharsets/UTF_8)))))
  (is (= {:status 200
          :headers {"content-type" "application/octet-stream"
                    "content-length" "16"}
          :body 16}
         (count-bytes (http-server/response 200 {"content-type" "application/octet-stream"} "Text, but binary"))))
  ;; file->path
  (is (= {:status 200
          :headers {"content-type" "text/plain"}
          :body (fs/path "project.clj")}
         (http-server/response 200 (io/file "project.clj"))))
  ;; resource: jar file url
  (is (= {:status 200
          :headers {"content-type" "text/plain"}
          :body (io/resource "cljfx/api.clj")}
         (http-server/response 200 (io/resource "cljfx/api.clj"))))
  ;; resource: file url
  (is (= {:status 200
          :body (io/resource "about.fxml")}
         (http-server/response 200 (io/resource "about.fxml"))))

  (test-util/with-loaded-project
    ;; editor resources: file->path
    (is (= {:status 200
            :headers {"content-type" "text/plain"}
            :body (fs/path (workspace/project-directory workspace) "game.project")}
           (http-server/response 200 (workspace/find-resource workspace "/game.project"))))
    ;; editor resources: zip as is
    (is (= {:status 200
            :headers {"content-type" "text/plain"}
            :body (workspace/find-resource workspace "/builtins/docs/licenses.md")}
           (http-server/response 200 (workspace/find-resource workspace "/builtins/docs/licenses.md"))))))

(defn- get-written-response [response & http-client-args]
  (with-open [server (http-server/start! (http-server/router-handler {"/" {"GET" (constantly response)}}))]
    (ignore-date @(apply http/request (http-server/url server) http-client-args))))

(defn- count-resource-bytes [resource]
  (let [baos (ByteArrayOutputStream.)
        _ (with-open [is (io/input-stream resource)]
            (io/copy is baos))]
    (count (.toByteArray baos))))

(deftest response-write-test
  ;; file
  (let [project-clj-size (fs/path-size (fs/path "project.clj"))]
    (is (= {:status 200
            :headers {"content-length" (str project-clj-size)
                      "content-type" "text/plain"}
            :body project-clj-size}
           (count-bytes (get-written-response (http-server/response 200 (io/file "project.clj")) :as :byte-array)))))
  ;; jar file resource
  (let [res (io/resource "cljfx/api.clj")
        size (count-resource-bytes res)]
    (is (= {:status 200
            :headers {"content-length" (str size)
                      "content-type" "text/plain"}
            :body size}
           (count-bytes (get-written-response (http-server/response 200 res) :as :byte-array)))))
  ;; file resource
  (let [res (io/resource "about.fxml")
        size (count-resource-bytes res)]
    (is (= {:status 200
            :headers {"content-length" (str size)
                      "content-type" "application/xml"}
            :body size}
           (count-bytes (get-written-response (http-server/response 200 res) :as :byte-array)))))
  (test-util/with-loaded-project
    ;; editor file resource
    (let [res (workspace/find-resource workspace "/game.project")
          size (count-resource-bytes res)]
      (is (= {:status 200
              :headers {"content-length" (str size)
                        "content-type" "text/plain"}
              :body size}
           (count-bytes (get-written-response (http-server/response 200 res) :as :byte-array)))))
    ;; editor zip resource
    (let [res (workspace/find-resource workspace "/builtins/docs/licenses.md")
          size (count-resource-bytes res)]
      (is (= {:status 200
              :headers {"content-length" (str size)
                        "content-type" "text/plain"}
              :body size}
             (count-bytes (get-written-response (http-server/response 200 res) :as :byte-array))))))
  ;; for some valid responses we can't infer the size beforehand
  (is (= {:status 200
          :headers {"transfer-encoding" "chunked"}
          :body "input stream"}
         (get-written-response
           (http-server/response
             200
             (ByteArrayInputStream. (.getBytes "input stream" StandardCharsets/UTF_8)))
           :as :string))))

(defn- schema-leaves
  ([schema]
   (schema-leaves [] schema))
  ([path schema]
   (case (:type schema)
     :object (e/cons
               [path schema]
               (e/mapcat
                 (fn [[k v]]
                   (schema-leaves (conj path k) v))
                 (:properties schema)))
     [[path schema]])))

(defn- default-value [schema]
  (if (contains? schema :default)
    (:default schema)
    (if (= :keyword (:type schema))
      :test
      (prefs/default-value schema))))

(defn- different-value [value schema]
  (case (:type schema)
    :boolean (not value)
    (:password :string :locale) (str value "++")
    :keyword (keyword (str (if value (name value) "foo") "++"))
    (:integer :number) (inc value)
    :one-of (let [[a b] (:schemas schema)]
              (if (prefs/valid? a value)
                (default-value b)
                (default-value a)))
    :array (let [item-schema (:item schema)
                 value (if (coll/empty? value)
                         [(default-value item-schema)]
                         value)]
             (mapv #(different-value % item-schema) value))
    :set (let [item-schema (:item schema)
               value (if (coll/empty? value)
                       #{(default-value item-schema)}
                       value)]
           (into #{} (map #(different-value % item-schema)) value))
    :object (coll/pair-map-by
              key
              (fn [[k v]]
                (different-value (k value (default-value v)) v))
              (:properties schema))
    :object-of (let [{:keys [key val]} schema
                     value (if (coll/empty? value)
                             {(default-value key) (default-value val)}
                             value)]
                 (into {}
                       (map (fn [[k v]]
                              (coll/pair (different-value k key) (different-value v val))))
                       value))
    :tuple (mapv different-value value (:items schema))
    :enum (let [{:keys [values]} schema
                [a b] values]
            (if (= value a) b a))))

(deftest prefs-endpoint-roundtrip-test
  (let [prefs (prefs/make :scopes {:global (fs/create-temp-file!)
                                   :project (fs/create-temp-file!)}
                          :schemas [:default])]
    (with-open [server (http-server/start! (http-server/router-handler (http-server.prefs/routes prefs)))]
      (let [url (http-server/local-url server)]
        (doseq [[path schema] (schema-leaves (prefs/schema prefs []))]
          (let [path-str (coll/join-to-string "/" (e/map name path))
                path-url (str url "/prefs/" path-str)]
            (testing path-str
              (let [first-get @(http/request path-url :as :string)]
                (when (is (= 200 (:status first-get)))
                  (let [first-json (json/read-str (:body first-get))
                        first-clj (prefs/get prefs path)]
                    (testing "write the same value"
                      (when (is (= 200 (:status @(http/request path-url :method "POST" :body (json/write-str first-json)))))
                        (let [second-get @(http/request path-url :as :string)]
                          (when (is (= 200 (:status second-get)))
                            (let [second-json (json/read-str (:body second-get))
                                  second-clj (prefs/get prefs path)]
                              (is (= first-json second-json))
                              (is (= first-clj second-clj)))))))
                    (testing "write a different value"
                      (let [new-value (different-value (prefs/get prefs path) schema)]
                        (when (is (= 200 (:status @(http/request path-url :method "POST" :body (json/write-str new-value)))))
                          (let [second-get @(http/request path-url :as :string)]
                            (when (is (= 200 (:status second-get)))
                              (let [second-json (json/read-str (:body second-get))
                                    second-clj (prefs/get prefs path)]
                                (is (not= first-json second-json))
                                (is (not= first-clj second-clj))
                                (is (= new-value second-clj))))))))))))))))))

(deftest web-server-test
  (test-util/with-loaded-project
    (let [root (doto (Region.)
                 (ui/context! :global
                              {:workspace workspace
                               :changes-view workspace}
                              (reify handler/SelectionProvider
                                (selection [_])
                                (succeeding-selection [_])
                                (alt-selection [_]))))
          view-graph (g/node-id->graph-id app-view)
          console (g/make-node! view-graph console/ConsoleNode)
          console-view (g/make-node! view-graph view/CodeEditorView :gutter-view (console/->ConsoleGutterView))]
      (g/connect! console :_node-id console-view :resource-node)
      (binding [ui/*main-stage* (atom @(fx/on-fx-thread (doto (Stage.) (.setScene (Scene. root)))))]
        (with-open [server (http-server/start!
                             (web-server/make-dynamic-handler
                               (into [] cat [(engine-profiler/routes)
                                             (console/routes console-view)
                                             (hot-reload/routes workspace)
                                             (bob/routes project)
                                             (command-requests/router root progress/null-render-progress!)])))]
          (let [url (http-server/local-url server)]
            (let [{:keys [status headers body]} @(http/request (str url "/engine-profiler/") :as :string)]
              (is (= 200 status))
              (is (= "text/html" (get headers "content-type")))
              (is (string/includes? body "<html")))
            (let [{:keys [status headers body]} @(http/request (str url "/engine-profiler/remotery/vis/Code/ThreadFrame.js") :as :string)]
              (is (= 200 status))
              (is (= "text/javascript" (get headers "content-type")))
              (is (string/includes? body "class ThreadFrame")))
            (let [{:keys [status headers body]} @(http/request (str url "/console") :as :string)]
              (is (= 200 status))
              (is (= "application/json" (get headers "content-type")))
              (is (string/includes? body "\"lines\":")))
            (let [{:keys [status headers body]} (update @(http/request (str url "/command/") :as :string) :body json/read-str :key-fn keyword)]
              (is (= 200 status))
              (is (= "application/json" (get headers "content-type")))
              (is (contains? body :build-html5)))
            (let [{:keys [status headers body]} @(http/request (str url "/command/build-html5") :as :string)]
              (is (= 405 status))
              (is (= "OPTIONS, POST" (get headers "allow")))
              (is (= "405 Method Not Allowed\n" body)))
            (with-redefs [ui/open-url (promise)]
              (let [{:keys [status body]} @(http/request (str url "/command/report-issue") :method "POST" :as :string)]
                (is (= 202 status))
                (is (= "202 Accepted\n" body)))
              (when (is (realized? ui/open-url))
                (is (string/includes? @ui/open-url "github.com/defold/defold/issues"))))))))))

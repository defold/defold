(ns integration.web-server-test
  (:require [cljfx.api :as fx]
            [clojure.data.json :as json]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.view :as view]
            [editor.console :as console]
            [editor.handler :as handler]
            [editor.progress :as progress]
            [editor.ui :as ui]
            [editor.web-server :as web-server]
            [integration.test-util :as test-util]
            [util.http-client :as http]
            [util.http-server :as http-server])
  (:import [javafx.scene Scene]
           [javafx.scene.layout Region]
           [javafx.stage Stage]))

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
        (test-util/with-server (web-server/handler workspace project console-view root progress/null-render-progress!)
          (let [url (http-server/local-url server)]
            (let [{:keys [status headers body]} @(http/request (str url "/engine-profiler/") :as :string)]
              (is (= 200 status))
              (is (= "text/html" (get headers "content-type")))
              (is (string/includes? body "<html")))
            (let [{:keys [status headers body]} @(http/request (str url "/engine-profiler/remotery/vis/Code/ThreadFrame.js") :as :string)]
              (is (= 200 status))
              (is (= "text/javascript" (get headers "content-type")))
              (is (string/includes? body "class ThreadFrame")))
            (let [{:keys [status headers body]} @(http/request (str url "/profiler") :as :string)]
              (is (= 200 status))
              (is (= "text/html" (get headers "content-type")))
              (is (string/includes? body "var data = [")))
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

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

(ns integration.lsp-test
  (:require [clojure.core.async :as a :refer [<! >!]]
            [clojure.data.json :as json]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.code.data :as data]
            [editor.defold-project :as project]
            [editor.lsp :as lsp]
            [editor.lsp.base :as lsp.base]
            [editor.lsp.jsonrpc :as lsp.jsonrpc]
            [editor.lsp.server :as lsp.server]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.async-support :as async-support]
            [support.test-support :as test-support])
  (:import [java.io PipedInputStream PipedOutputStream]))

(set! *warn-on-reflection* true)

(defmacro await-lsp [& forms]
  `(test-util/with-ui-run-later-rebound
     (let [result# (do ~@forms)]
       (Thread/sleep 100)
       result#)))

(defn- set-servers! [lsp new-servers]
  (await-lsp
    (lsp/set-servers! lsp new-servers)))

(defn- open-view! [lsp view-node resource lines]
  (await-lsp
    (lsp/open-view! lsp view-node resource lines)))

(defn- close-view! [lsp view-node]
  (await-lsp
    (lsp/close-view! lsp view-node)))

(defn- edit-file! [code-resource-node-id lines-or-text]
  (await-lsp
    (test-util/set-code-editor-source! code-resource-node-id lines-or-text)))

(defn- rename-file! [resource new-file-name]
  (await-lsp
    (asset-browser/rename resource new-file-name test-util/localization)))

(defn- delete-file! [resource]
  (await-lsp
    (asset-browser/delete [resource])))

(defn- resource-sync! [workspace]
  (await-lsp
    (workspace/resource-sync! workspace)))

(defn- handler-run! [command project]
  (await-lsp
    (test-util/handler-run command [{:name :global :env {:project-graph (project/graph project)}}] {})))

(def ^:private undo! (partial handler-run! :edit.undo))

(def ^:private redo! (partial handler-run! :edit.redo))

(defn- pull-diagnostics! [lsp & args]
  (await-lsp
    (let [ret (promise)]
      (apply lsp/pull-workspace-diagnostics! lsp ret args)
      @ret)))

(defn- hover! [lsp resource cursor]
  (await-lsp
    (let [ret (promise)]
      (lsp/hover! lsp resource cursor ret)
      @ret)))

(defn- prepare-rename [lsp resource cursor]
  (await-lsp
    (let [ret (promise)]
      (lsp/prepare-rename lsp resource cursor ret)
      @ret)))

(defn- rename [lsp prepared-range new-name]
  (await-lsp
    (let [ret (promise)]
      (lsp/rename lsp prepared-range new-name ret)
      @ret)))

(defn- make-test-server-launcher [request-handlers]
  (reify lsp.server/Launcher
    (launch [_ _]
      (let [private-out (PipedOutputStream.)
            public-in (PipedInputStream. private-out)
            private-in (PipedInputStream.)
            public-out (PipedOutputStream. private-in)
            [base-source base-sink] (lsp.base/make private-in private-out)
            notify-client! (fn notify-client! [method params]
                             (let [message (cond-> {:jsonrpc "2.0"
                                                    :method method}
                                                   params
                                                   (assoc :params params))]
                               (a/put! base-sink (json/write-str message))))]
        (lsp.jsonrpc/make (fn get-request-method [method]
                            (when-let [f (request-handlers method)]
                              (fn respond [params]
                                (f params notify-client!))))
                          base-source
                          base-sink)
        (reify lsp.server/Connection
          (input-stream [_] public-in)
          (output-stream [_] public-out)
          (dispose [_]
            (.close public-in)
            (.close public-out)
            (.close private-in)
            (.close private-out)))))))

(def default-handlers
  {"initialize" (fn [_ _] {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-incremental}})
   "textDocument/didOpen" (fn [{{:keys [uri]} :textDocument} notify!]
                            (notify! "textDocument/publishDiagnostics"
                                     {:uri uri
                                      :diagnostics [{:range {:start {:line 0 :character 0}
                                                             :end {:line 0 :character 1}}
                                                     :message "It's a bad start!"
                                                     :severity 1}]}))
   "initialized" (constantly nil)
   "shutdown" (constantly nil)
   "exit" (constantly nil)})

(def ^:private foo-json-lines ["{\"asd\": 1}"])

(deftest lsp-server-test
  (testing "Initialize + open text document -> should publish diagnostics"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [in (a/chan 10)
            out (a/chan 10)]
        (await-lsp
          (lsp.server/make
            project
            (make-test-server-launcher default-handlers)
            in out
            :on-publish-diagnostics #(apply vector :on-publish-diagnostics %&)
            :on-initialized #(vector :on-initialized %)))
        (is (= [[:on-initialized
                 {:text-document-sync {:open-close true
                                       :change :incremental}
                  :pull-diagnostics :none
                  :goto-definition false
                  :find-references false
                  :hover false
                  :rename false}]
                [:on-publish-diagnostics
                 (test-util/resource workspace "/foo.json")
                 {:items [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                            :message "It's a bad start!" :severity :error)]}]]
               (await-lsp
                 (async-support/eventually
                   (a/go
                     (>! in (lsp.server/open-text-document (test-util/resource workspace "/foo.json") foo-json-lines))
                     (<! (a/timeout 10))
                     (a/close! in)
                     (<! (a/reduce conj [] out)))))))))))

(g/defnode LSPViewNode
  (property diagnostics g/Any (default []))
  (property completion-trigger-characters g/Any (default #{})))

(deftest start-open-order-test
  (test-util/with-scratch-project "test/resources/lsp_project"
    (let [lsp (lsp/get-node-lsp project)]
      (testing "Start server + open resource -> should receive diagnostics"
        (let [;; set servers
              _ (set-servers! lsp #{{:languages #{"json"}
                                     :launcher (make-test-server-launcher default-handlers)}})
              ;; open view
              view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
              _ (open-view! lsp view-node (test-util/resource workspace "/foo.json") foo-json-lines)]
          (is (= [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                    :type :diagnostic :hoverable true :messages ["It's a bad start!"] :severity :error)]
                 (g/node-value view-node :diagnostics)))
          (close-view! lsp view-node)))
      (testing "Open resource + start server -> should receive diagnostics"
        (let [;; open view
              view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
              _ (open-view! lsp view-node (test-util/resource workspace "/foo.json") foo-json-lines)
              ;; set servers
              _ (set-servers! lsp #{{:languages #{"json"}
                                     :launcher (make-test-server-launcher default-handlers)}})]
          (is (= [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                    :type :diagnostic :hoverable true :messages ["It's a bad start!"] :severity :error)]
                 (g/node-value view-node :diagnostics)))
          (close-view! lsp view-node))))))

(deftest text-sync-kind-test
  (testing "Respect language server text sync kind capabilities"
    (let [change-notifications (atom #{})
          make-handlers (fn [sync-kind]
                          {"initialize" (fn [_ _] {:capabilities {:textDocumentSync sync-kind}})
                           "initialized" (constantly nil)
                           "textDocument/didOpen" (constantly nil)
                           "textDocument/didChange" (fn [v _]
                                                      (swap! change-notifications conj [sync-kind v]))
                           "shutdown" (constantly nil)
                           "exit" (constantly nil)})]
      (test-util/with-scratch-project "test/resources/lsp_project"
        (let [lsp (lsp/get-node-lsp project)
              _ (set-servers!
                  lsp
                  #{{:languages #{"json"}
                     :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-incremental))}
                    {:languages #{"json"}
                     :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-full))}
                    {:languages #{"json"}
                     :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-none))}})
              view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
              foo-resource (test-util/resource workspace "/foo.json")
              foo-node (test-util/resource-node project "/foo.json")
              lines (g/node-value foo-node :lines)
              _ (open-view! lsp view-node (test-util/resource workspace "/foo.json") lines)
              _ (edit-file!
                  foo-node
                  (data/splice-lines lines {data/document-start-cursor-range ["NEWTEXT"]}))]
          (is (= #{[lsp.server/lsp-text-document-sync-kind-incremental
                    {:textDocument {:uri (lsp.server/resource-uri foo-resource)
                                    :version 1}
                     :contentChanges [{:range {:start {:line 0
                                                       :character 0}
                                               :end {:line 0
                                                     :character 0}}
                                       :text "NEWTEXT"}]}]
                   [lsp.server/lsp-text-document-sync-kind-full
                    {:textDocument {:uri (lsp.server/resource-uri foo-resource)
                                    :version 1}
                     :contentChanges [{:text "NEWTEXT{\"asd\": 1}"}]}]}
                 @change-notifications))
          (edit-file! foo-node lines)
          (close-view! lsp view-node))))))

(deftest polled-resources-test
  (testing "Modifying resources without any views should make the language servers open the document anyway"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [server-opened-docs (atom #{})
            lsp (lsp/get-node-lsp project)
            foo-resource (test-util/resource workspace "/foo.json")
            initial-source (slurp foo-resource)
            foo-node (test-util/resource-node project "/foo.json")
            _ (g/set-graph-value! (project/graph project) ::the-graph ::test)
            _ (set-servers!
                lsp
                #{{:languages #{"json"}
                   :launcher (make-test-server-launcher
                               {"initialize" (constantly {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-incremental}})
                                "initialized" (constantly nil)
                                "textDocument/didOpen" (fn [{:keys [textDocument]} _]
                                                         (swap! server-opened-docs conj (:uri textDocument)))
                                "textDocument/didClose" (fn [{:keys [textDocument]} _]
                                                          (swap! server-opened-docs disj (:uri textDocument)))
                                "shutdown" (constantly nil)
                                "exit" (constantly nil)})}})

            ;; modify => dirty
            _ (edit-file! foo-node "{}")
            _ (is (= #{(lsp.server/resource-uri foo-resource)} @server-opened-docs))

            ;; modify to initial => clean
            _ (edit-file! foo-node initial-source)
            _ (is (= #{} @server-opened-docs))

            ;; undo => dirty again
            _ (undo! project)
            _ (is (= #{(lsp.server/resource-uri foo-resource)} @server-opened-docs))

            ;; redo => clean again
            _ (redo! project)
            _ (is (= #{} @server-opened-docs))]))))

(deftest open-close-test
  (test-util/with-scratch-project "test/resources/lsp_project"
    (let [lsp (lsp/get-node-lsp project)
          server-opened-docs (atom #{})
          handlers {"initialize" (constantly {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-incremental}})
                    "initialized" (constantly nil)
                    "textDocument/didOpen" (fn [{:keys [textDocument]} _]
                                             (swap! server-opened-docs conj (:uri textDocument)))
                    "textDocument/didClose" (fn [{:keys [textDocument]} _]
                                              (swap! server-opened-docs disj (:uri textDocument)))
                    "shutdown" (constantly nil)
                    "exit" (constantly nil)}]
      (testing "Open view -> notify open, close view -> notify close"
        (let [_ (set-servers!
                  lsp #{{:languages #{"json"}
                         :launcher (make-test-server-launcher handlers)}})
              view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
              foo-resource (test-util/resource workspace "/foo.json")
              foo-resource-uri (lsp.server/resource-uri foo-resource)
              ;; open view
              _ (open-view! lsp view-node foo-resource foo-json-lines)
              _ (is (= #{foo-resource-uri} @server-opened-docs))
              ;; close view
              _ (close-view! lsp view-node)
              _ (is (= #{} @server-opened-docs))]))
      (testing "Open view -> notify open, modify lines + close view -> still open"
        (let [_ (set-servers!
                  lsp
                  #{{:languages #{"json"}
                     :launcher (make-test-server-launcher handlers)}})
              view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
              foo-resource (test-util/resource workspace "/foo.json")
              foo-resource-node (test-util/resource-node project "/foo.json")
              foo-resource-uri (lsp.server/resource-uri foo-resource)
              ;; open view
              _ (open-view! lsp view-node foo-resource foo-json-lines)
              _ (is (= #{foo-resource-uri} @server-opened-docs))
              ;; modify lines, close view
              _ (edit-file! foo-resource-node "{}")
              _ (close-view! lsp view-node)
              _ (is (= #{foo-resource-uri} @server-opened-docs))])))))

(deftest resource-changes-test
  (testing "Modify lines -> notify open, rename file -> close + open modified"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [lsp (lsp/get-node-lsp project)
            server-opened-docs (atom {})
            handlers {"initialize" (constantly {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-incremental}})
                      "initialized" (constantly nil)
                      "textDocument/didOpen" (fn [{:keys [textDocument]} _]
                                               (swap! server-opened-docs assoc (:uri textDocument) (:text textDocument)))
                      "textDocument/didClose" (fn [{:keys [textDocument]} _]
                                                (swap! server-opened-docs dissoc (:uri textDocument)))
                      "shutdown" (constantly nil)
                      "exit" (constantly nil)}
            _ (set-servers!
                lsp
                #{{:languages #{"json"}
                   :launcher (make-test-server-launcher handlers)}})

            foo-resource (test-util/resource workspace "/foo.json")
            old-foo-content (slurp foo-resource)
            foo-resource-node (test-util/resource-node project "/foo.json")
            foo-resource-uri (lsp.server/resource-uri foo-resource)

            ;; modify
            _ (edit-file! foo-resource-node "{}")
            _ (is (= {foo-resource-uri "{}"} @server-opened-docs))

            ;; rename foo.json to bar.json
            _ (rename-file! [foo-resource] "bar")
            bar-resource (test-util/resource workspace "/bar.json")
            bar-resource-uri (lsp.server/resource-uri bar-resource)
            _ (is (= {bar-resource-uri "{}"} @server-opened-docs))]
        (edit-file! (test-util/resource-node project "/bar.json") old-foo-content)
        (rename-file! [bar-resource] "foo"))))
  (testing "Open view -> notify open, change on disk + resource sync -> notify changed"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [lsp (lsp/get-node-lsp project)
            change-notifications (atom [])
            server-opened-docs (atom #{})
            handlers {"initialize" (constantly {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-incremental}})
                      "initialized" (constantly nil)
                      "textDocument/didOpen" (fn [{:keys [textDocument]} _]
                                               (swap! server-opened-docs conj (:uri textDocument)))
                      "textDocument/didChange" (fn [v _]
                                                 (swap! change-notifications conj v))
                      "textDocument/didClose" (fn [{:keys [textDocument]} _]
                                                (swap! server-opened-docs disj (:uri textDocument)))
                      "shutdown" (constantly nil)
                      "exit" (constantly nil)}
            _ (set-servers!
                lsp
                #{{:languages #{"json"}
                   :launcher (make-test-server-launcher handlers)}})
            view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
            foo-resource (test-util/resource workspace "/foo.json")
            old-foo-content (slurp foo-resource)
            foo-resource-node (test-util/resource-node project "/foo.json")
            foo-resource-uri (lsp.server/resource-uri foo-resource)
            lines (g/node-value foo-resource-node :lines)
            ;; open view
            _ (open-view! lsp view-node foo-resource lines)
            _ (is (= #{foo-resource-uri} @server-opened-docs))
            ;; modify on disk + sync
            _ (test-support/spit-until-new-mtime foo-resource "NEW_CONTENT")
            _ (resource-sync! workspace)
            _ (is (= [{:textDocument {:uri foo-resource-uri :version 1}
                       :contentChanges [{:text "NEW_CONTENT"}]}]
                     @change-notifications))]
        (test-support/spit-until-new-mtime foo-resource old-foo-content)
        (resource-sync! workspace)
        (close-view! lsp view-node))))
  (testing "Modify lines -> notify open; delete file + sync -> notify closed"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [lsp (lsp/get-node-lsp project)
            server-opened-docs (atom #{})
            handlers {"initialize" (constantly {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-incremental}})
                      "initialized" (constantly nil)
                      "textDocument/didOpen" (fn [{:keys [textDocument]} _]
                                               (swap! server-opened-docs conj (:uri textDocument)))
                      "textDocument/didClose" (fn [{:keys [textDocument]} _]
                                                (swap! server-opened-docs disj (:uri textDocument)))
                      "shutdown" (constantly nil)
                      "exit" (constantly nil)}
            _ (set-servers!
                lsp
                #{{:languages #{"json"}
                   :launcher (make-test-server-launcher handlers)}})
            foo-resource (test-util/resource workspace "/foo.json")
            old-foo-content (slurp foo-resource)
            foo-resource-node (test-util/resource-node project "/foo.json")
            foo-resource-uri (lsp.server/resource-uri foo-resource)
            ;; modify lines
            _ (edit-file! foo-resource-node "{}")
            _ (is (= #{foo-resource-uri} @server-opened-docs))
            ;; delete file
            _ (delete-file! foo-resource)
            _ (is (= #{} @server-opened-docs))]
        (test-support/spit-until-new-mtime foo-resource old-foo-content)
        (resource-sync! workspace)))))

(deftest workspace-diagnostics-test
  (testing "Workspace diagnostics with different pull diagnostics kinds"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [workspace-lint-exit-promise (promise)
            document-lint-exit-promise (promise)
            no-lint-exit-promise (promise)
            lsp (lsp/get-node-lsp project)
            _ (set-servers!
                lsp
                #{;; full workspace lint
                  {:languages #{"json"}
                   :launcher
                   (make-test-server-launcher
                     {"initialize" (constantly {:capabilities {:diagnosticProvider {:workspaceDiagnostics true}}})
                      "workspace/diagnostic" (fn [_ _]
                                               {:items [{:uri (lsp.server/resource-uri (workspace/find-resource workspace "/foo.json"))
                                                         :kind "full"
                                                         :items [{:range {:start {:line 0 :character 0}
                                                                          :end {:line 0 :character 1}}
                                                                  :message "Workspace diagnostics error"
                                                                  :severity 1}]}]})
                      "initialized" (constantly nil)
                      "shutdown" (constantly nil)
                      "exit" (fn [_ _]
                               (deliver workspace-lint-exit-promise true))})}
                  ;; document lint
                  {:languages #{"json"}
                   :launcher
                   (make-test-server-launcher
                     {"initialize" (constantly {:capabilities {:diagnosticProvider {:workspaceDiagnostics false}}})
                      "textDocument/diagnostic" (fn [_ _]
                                                  {:kind "full"
                                                   :items [{:range {:start {:line 0 :character 1}
                                                                    :end {:line 0 :character 2}}
                                                            :message "Text document diagnostics error"
                                                            :severity 1}]})
                      "initialized" (constantly nil)
                      "shutdown" (constantly nil)
                      "exit" (fn [_ _]
                               (deliver document-lint-exit-promise true))})}
                  ;; no lint at all
                  {:languages #{"json"}
                   :launcher
                   (make-test-server-launcher
                     {"initialize" (constantly {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-none}})
                      "initialized" (constantly nil)
                      "shutdown" (constantly nil)
                      "exit" (fn [_ _]
                               (deliver no-lint-exit-promise true))})}})]
        (is (= {(workspace/find-resource workspace "/foo.json")
                (sorted-set
                  (data/map->CursorRange
                    {:from (data/->Cursor 0 0)
                     :to (data/->Cursor 0 1)
                     :message "Workspace diagnostics error"
                     :severity :error})
                  (data/map->CursorRange
                    {:from (data/->Cursor 0 1)
                     :to (data/->Cursor 0 2)
                     :message "Text document diagnostics error"
                     :severity :error}))}
               (pull-diagnostics! lsp)))
        (await-lsp
          (set-servers! lsp #{})
          (is (true? (deref workspace-lint-exit-promise 200 false)))
          (is (true? (deref document-lint-exit-promise 200 false)))
          (is (true? (deref no-lint-exit-promise 200 false)))))))
  (testing "Failing server does not block workspace diagnostics"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [working-exit-promise (promise)
            broken-exit-promise (promise)
            lsp (lsp/get-node-lsp project)
            _ (set-servers!
                lsp
                #{;; working lint
                  {:languages #{"json"}
                   :launcher
                   (make-test-server-launcher
                     {"initialize" (constantly {:capabilities {:diagnosticProvider {:workspaceDiagnostics true}}})
                      "workspace/diagnostic" (fn [_ _]
                                               {:items [{:uri (lsp.server/resource-uri (workspace/find-resource workspace "/foo.json"))
                                                         :kind "full"
                                                         :items [{:range {:start {:line 0 :character 0}
                                                                          :end {:line 0 :character 1}}
                                                                  :message "It's a bad start!"
                                                                  :severity 1}]}]})
                      "initialized" (constantly nil)
                      "shutdown" (constantly nil)
                      "exit" (fn [_ _]
                               (deliver working-exit-promise true))})}
                  ;; broken: fails on diagnostic request
                  {:languages #{"json"}
                   :launcher
                   (make-test-server-launcher
                     {"initialize" (constantly {:capabilities {:diagnosticProvider {:workspaceDiagnostics true}}})
                      "initialized" (constantly nil)
                      "workspace/diagnostic" (fn [_ _] (throw (ex-info "This exception should be correctly handled by lsp test" {})))
                      "shutdown" (constantly nil)
                      "exit" (fn [_ _]
                               (deliver broken-exit-promise true))})}})]
        (is (= {(workspace/find-resource workspace "/foo.json")
                (sorted-set (data/map->CursorRange
                              {:from (data/->Cursor 0 0)
                               :to (data/->Cursor 0 1)
                               :message "It's a bad start!"
                               :severity :error}))}
               (pull-diagnostics! lsp)))
        (await-lsp
          (set-servers! lsp #{})
          (is (true? (deref working-exit-promise 200 false)))
          (is (true? (deref broken-exit-promise 200 false)))))))
  (testing "the LSP client only waits up to a timeout"
    (test-util/with-scratch-project "test/resources/lsp_project"
      (let [exit-promise (promise)
            lsp (lsp/get-node-lsp project)
            _ (set-servers!
                lsp
                #{{:languages #{"json"}
                   :launcher (make-test-server-launcher
                               {"initialize" (constantly {:capabilities {:diagnosticProvider {:workspaceDiagnostics true}}})
                                "initialized" (constantly nil)
                                "workspace/diagnostic" (fn [_ _]
                                                         (Thread/sleep 1000)
                                                         {:items [{:uri (lsp.server/resource-uri (workspace/find-resource workspace "/foo.json"))
                                                                   :kind "full"
                                                                   :items [{:range {:start {:line 0 :character 0}
                                                                                    :end {:line 0 :character 1}}
                                                                            :message "It's a bad start!"
                                                                            :severity 1}]}]})
                                "shutdown" (constantly nil)
                                "exit" (fn [_ _]
                                         (deliver exit-promise true))})}})]
        (is (nil? (pull-diagnostics! lsp :timeout-ms 500)))
        (await-lsp
          (set-servers! lsp #{})
          (is (true? (deref exit-promise 1100 false))))))))

(deftest hover-test
  (test-util/with-scratch-project "test/resources/lsp_project"
    (let [unmatched-promise (promise)
          matched-promise (promise)
          lsp (lsp/get-node-lsp project)
          _ (set-servers! lsp #{;; this server should NOT be asked for hovers
                                {:languages #{"json"}
                                 :launcher (make-test-server-launcher
                                             {"initialize" (constantly {:capabilities {:hoverProvider false}})
                                              "initialized" (constantly nil)
                                              "shutdown" (constantly nil)
                                              "textDocument/hover" (fn [request _]
                                                                     (deliver unmatched-promise request))
                                              "exit" (constantly nil)})}
                                ;; this server SHOULD be asked for hovers
                                {:languages #{"json"}
                                 :launcher (make-test-server-launcher
                                             {"initialize" (constantly {:capabilities {:hoverProvider true}})
                                              "initialized" (constantly nil)
                                              "shutdown" (constantly nil)
                                              "textDocument/hover" (fn [request _]
                                                                     (deliver matched-promise request)
                                                                     {:contents {:kind :markdown :value "hover"}})
                                              "exit" (constantly nil)})}})]
      (is (= [(data/map->CursorRange {:from (data/->Cursor 0 1)
                                      :to (data/->Cursor 0 2)
                                      :type :hover
                                      :hoverable true
                                      :content (lsp.server/->MarkupContent :markdown "hover")})]
             (hover! lsp (test-util/resource workspace "/foo.json") (data/->Cursor 0 1))))
      (is (realized? matched-promise))
      (is (not (realized? unmatched-promise)))
      (await-lsp (set-servers! lsp #{})))))

(deftest rename-test
  (test-util/with-scratch-project "test/resources/lsp_project"
    (let [lsp (lsp/get-node-lsp project)
          _ (set-servers! lsp #{{:languages #{"json"}
                                 :launcher (make-test-server-launcher
                                             {"initialize" (constantly {:capabilities {:renameProvider {:prepareProvider true}}})
                                              "initialized" (constantly nil)
                                              "textDocument/prepareRename" (fn [{:keys [position]} _]
                                                                             {:range {:start position
                                                                                      :end (update position :character inc)}})
                                              "textDocument/rename" (fn [{:keys [position newName textDocument]} _]
                                                                      {:changes {(:uri textDocument) [{:range {:start position
                                                                                                               :end (update position :character inc)}
                                                                                                       :newText newName}]}})
                                              "shutdown" (constantly nil)
                                              "exit" (constantly nil)})}})]
      (let [resource (test-util/resource workspace "/foo.json")
            rename-region (prepare-rename lsp resource (data/->Cursor 0 0))]
        (is (= #code/range[[0 0] [0 1]] rename-region))
        (is (= {resource [[#code/range [[0 0] [0 1]] ["foo"]]]}
               (rename lsp rename-region "foo")))
        (await-lsp (set-servers! lsp #{}))))))

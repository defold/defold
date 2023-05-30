;; Copyright 2020-2022 The Defold Foundation
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
            [editor.code.data :as data]
            [editor.asset-browser :as asset-browser]
            [editor.defold-project :as project]
            [editor.lsp :as lsp]
            [editor.lsp.base :as lsp.base]
            [editor.lsp.jsonrpc :as lsp.jsonrpc]
            [editor.lsp.server :as lsp.server]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [integration.test-util :as tu]
            [support.async-support :as async-support]
            [support.test-support :as test-support])
  (:import [java.io PipedInputStream PipedOutputStream]))

(set! *warn-on-reflection* true)

(defn- make-test-server-launcher [request-handlers]
  (reify lsp.server/Launcher
    (launch [_ _]
      (let [private-out (PipedOutputStream.)
            public-in (PipedInputStream. private-out)
            private-in (PipedInputStream.)
            public-out (PipedOutputStream. private-in)
            [base-source base-sink] (lsp.base/make private-in private-out)
            notify-client! (fn [method params]
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

(deftest lsp-server-test
  (testing "Initialize + open text document -> should publish diagnostics"
    (tu/with-loaded-project "test/resources/lsp_project"
      (let [in (a/chan 10)
            out (a/chan 10)]
        (lsp.server/make
          project
          (make-test-server-launcher default-handlers)
          in out
          :on-publish-diagnostics #(apply vector :on-publish-diagnostics %&)
          :on-initialized #(vector :on-initialized %))
        (is (= [[:on-initialized
                 {:text-document-sync {:open-close true
                                       :change :incremental}
                  :pull-diagnostics :none
                  :goto-definition false
                  :find-references false}]
                [:on-publish-diagnostics
                 (tu/resource workspace "/foo.json")
                 {:items [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                            :message "It's a bad start!" :severity :error)]}]]
               (async-support/eventually
                 (a/go
                   (>! in (lsp.server/open-text-document (tu/resource workspace "/foo.json") ["{\"a\": 1}"]))
                   (<! (a/timeout 10))
                   (a/close! in)
                   (<! (a/reduce conj [] out))))))))))

(g/defnode LSPViewNode
  (property diagnostics g/Any (default [])))

(deftest start-open-order-test
  (tu/with-loaded-project "test/resources/lsp_project"
    (with-redefs [ui/do-run-later (bound-fn [f] (f))]
      (let [lsp (lsp/get-node-lsp project)]
        (testing "Start server + open resource -> should receive diagnostics"
          (let [;; set servers
                _ (lsp/set-servers! lsp #{{:languages #{"json"}
                                           :launcher (make-test-server-launcher default-handlers)}})
                _ (Thread/sleep 100)
                ;; open view
                view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
                _ (lsp/open-view! lsp view-node (tu/resource workspace "/foo.json") ["{\"a\": 1}"])
                _ (Thread/sleep 100)]
            (is (= [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                      :type :diagnostic :hoverable true :messages ["It's a bad start!"] :severity :error)]
                   (g/node-value view-node :diagnostics)))
            (lsp/close-view! lsp view-node)))
        (testing "Open resource + start server -> should receive diagnostics"
          (let [;; open view
                view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
                _ (lsp/open-view! lsp view-node (tu/resource workspace "/foo.json") ["{\"a\": 1}"])
                _ (Thread/sleep 100)
                ;; set servers
                _ (lsp/set-servers! lsp #{{:languages #{"json"}
                                           :launcher (make-test-server-launcher default-handlers)}})
                _ (Thread/sleep 100)]
            (is (= [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                      :type :diagnostic :hoverable true :messages ["It's a bad start!"] :severity :error)]
                   (g/node-value view-node :diagnostics)))
            (lsp/close-view! lsp view-node)))))))

(deftest text-sync-kind-test
  (testing "Respect language server text sync kind capabilities"
    (with-redefs [ui/do-run-later (fn [f] (f))]
      (let [change-notifications (atom #{})
            make-handlers (fn [sync-kind]
                            {"initialize" (fn [_ _] {:capabilities {:textDocumentSync sync-kind}})
                             "initialized" (constantly nil)
                             "textDocument/didOpen" (constantly nil)
                             "textDocument/didChange" (fn [v _]
                                                        (swap! change-notifications conj [sync-kind v]))
                             "shutdown" (constantly nil)
                             "exit" (constantly nil)})]
        (tu/with-loaded-project "test/resources/lsp_project"
          (let [lsp (lsp/get-node-lsp project)
                _ (lsp/set-servers!
                    lsp
                    #{{:languages #{"json"}
                       :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-incremental))}
                      {:languages #{"json"}
                       :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-full))}
                      {:languages #{"json"}
                       :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-none))}})
                _ (Thread/sleep 100)
                view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
                foo-resource (tu/resource workspace "/foo.json")
                foo-node (tu/resource-node project "/foo.json")
                lines (g/node-value foo-node :lines)
                _ (lsp/open-view! lsp view-node (tu/resource workspace "/foo.json") lines)
                _ (g/set-property!
                    foo-node
                    :modified-lines
                    (data/splice-lines lines {data/document-start-cursor-range ["NEWTEXT"]}))
                _ (Thread/sleep 100)]
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
            (g/set-property! foo-node :modified-lines lines)
            (lsp/close-view! lsp view-node)))))))

(deftest polled-resources-test
  (with-redefs [ui/do-run-later (fn [f] (f))]
    (testing "Modifying resources without any views should make the language servers open the document anyway"
      (tu/with-loaded-project "test/resources/lsp_project"
        (let [server-opened-docs (atom #{})
              lsp (lsp/get-node-lsp project)
              foo-resource (tu/resource workspace "/foo.json")
              initial-source (slurp foo-resource)
              foo-node (tu/resource-node project "/foo.json")
              _ (g/set-graph-value! (project/graph project) ::the-graph ::test)
              _ (lsp/set-servers!
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
              _ (tu/code-editor-source! foo-node "{}")
              _ (Thread/sleep 100)
              _ (is (= #{(lsp.server/resource-uri foo-resource)} @server-opened-docs))

              ;; modify to initial => clean
              _ (tu/code-editor-source! foo-node initial-source)
              _ (Thread/sleep 100)
              _ (is (= #{} @server-opened-docs))

              ;; undo => dirty again
              _ (tu/handler-run :undo [{:name :global :env {:project-graph (project/graph project)}}] {})
              _ (Thread/sleep 100)
              _ (is (= #{(lsp.server/resource-uri foo-resource)} @server-opened-docs))

              ;; redo => clean again
              _ (tu/handler-run :redo [{:name :global :env {:project-graph (project/graph project)}}] {})
              _ (Thread/sleep 100)
              _ (is (= #{} @server-opened-docs))])))))

(deftest open-close-test
  (with-redefs [ui/do-run-later (fn [f] (f))]
    (tu/with-loaded-project "test/resources/lsp_project"
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
          (let [_ (lsp/set-servers! lsp #{{:languages #{"json"}
                                           :launcher (make-test-server-launcher handlers)}})
                view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
                foo-resource (tu/resource workspace "/foo.json")
                foo-resource-uri (lsp.server/resource-uri foo-resource)
                ;; open view
                _ (lsp/open-view! lsp view-node foo-resource ["{\"a\": 1}"])
                _ (Thread/sleep 100)
                _ (is (= #{foo-resource-uri} @server-opened-docs))
                ;; close view
                _ (lsp/close-view! lsp view-node)
                _ (Thread/sleep 100)
                _ (is (= #{} @server-opened-docs))]))
        (testing "Open view -> notify open, modify lines + close view -> still open"
          (let [_ (lsp/set-servers!
                    lsp
                    #{{:languages #{"json"}
                       :launcher (make-test-server-launcher handlers)}})
                view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
                foo-resource (tu/resource workspace "/foo.json")
                foo-resource-node (tu/resource-node project "/foo.json")
                foo-resource-uri (lsp.server/resource-uri foo-resource)
                ;; open view
                _ (lsp/open-view! lsp view-node foo-resource ["{\"a\": 1}"])
                _ (Thread/sleep 100)
                _ (is (= #{foo-resource-uri} @server-opened-docs))
                ;; modify lines, close view
                _ (tu/code-editor-source! foo-resource-node "{}")
                _ (lsp/close-view! lsp view-node)
                _ (Thread/sleep 100)
                _ (is (= #{foo-resource-uri} @server-opened-docs))]))))))

(deftest resource-changes-test
  (testing "Modify lines -> notify open, rename file -> close + open modified"
    (tu/with-loaded-project "test/resources/lsp_project"
      (with-redefs [ui/do-run-later (fn [f] (f))]
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
              _ (lsp/set-servers!
                  lsp
                  #{{:languages #{"json"}
                     :launcher (make-test-server-launcher handlers)}})

              foo-resource (tu/resource workspace "/foo.json")
              old-foo-content (slurp foo-resource)
              foo-resource-node (tu/resource-node project "/foo.json")
              foo-resource-uri (lsp.server/resource-uri foo-resource)

              ;; modify
              _ (tu/code-editor-source! foo-resource-node "{}")
              _ (Thread/sleep 100)
              _ (is (= {foo-resource-uri "{}"} @server-opened-docs))

              ;; rename foo.json to bar.json
              _ (asset-browser/rename foo-resource "bar.json")
              bar-resource (tu/resource workspace "/bar.json")
              bar-resource-uri (lsp.server/resource-uri bar-resource)
              _ (Thread/sleep 100)
              _ (is (= {bar-resource-uri "{}"} @server-opened-docs))]
          (tu/code-editor-source! (tu/resource-node project "/bar.json") old-foo-content)
          (asset-browser/rename bar-resource "foo.json")
          nil))))
  (testing "Open view -> notify open, change on disk + resource sync -> notify changed"
    (tu/with-loaded-project "test/resources/lsp_project"
      (with-redefs [ui/do-run-later (fn [f] (f))]
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
              _ (lsp/set-servers!
                  lsp
                  #{{:languages #{"json"}
                     :launcher (make-test-server-launcher handlers)}})
              view-node (g/make-node! (g/node-id->graph-id app-view) LSPViewNode)
              foo-resource (tu/resource workspace "/foo.json")
              old-foo-content (slurp foo-resource)
              foo-resource-node (tu/resource-node project "/foo.json")
              foo-resource-uri (lsp.server/resource-uri foo-resource)
              lines (g/node-value foo-resource-node :lines)
              ;; open view
              _ (lsp/open-view! lsp view-node foo-resource lines)
              _ (Thread/sleep 100)
              _ (is (= #{foo-resource-uri} @server-opened-docs))
              ;; modify on disk + sync
              _ (test-support/spit-until-new-mtime foo-resource "NEW_CONTENT")
              _ (workspace/resource-sync! workspace)
              _ (Thread/sleep 100)
              _ (is (= [{:textDocument {:uri foo-resource-uri :version 1}
                         :contentChanges [{:text "NEW_CONTENT"}]}]
                       @change-notifications))]
          (test-support/spit-until-new-mtime foo-resource old-foo-content)
          (workspace/resource-sync! workspace)
          (lsp/close-view! lsp view-node)))))
  (testing "Modify lines -> notify open; delete file + sync -> notify closed"
    (tu/with-loaded-project "test/resources/lsp_project"
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
            _ (lsp/set-servers!
                lsp
                #{{:languages #{"json"}
                   :launcher (make-test-server-launcher handlers)}})
            foo-resource (tu/resource workspace "/foo.json")
            old-foo-content (slurp foo-resource)
            foo-resource-node (tu/resource-node project "/foo.json")
            foo-resource-uri (lsp.server/resource-uri foo-resource)
            ;; modify lines
            _ (tu/code-editor-source! foo-resource-node "{}")
            _ (Thread/sleep 100)
            _ (is (= #{foo-resource-uri} @server-opened-docs))
            ;; delete file
            _ (asset-browser/delete [foo-resource])
            _ (Thread/sleep 100)
            _ (is (= #{} @server-opened-docs))]
        (test-support/spit-until-new-mtime foo-resource old-foo-content)
        (workspace/resource-sync! workspace)))))

(deftest workspace-diagnostics-test
  (testing "Workspace diagnostics with different pull diagnostics kinds"
    (tu/with-loaded-project "test/resources/lsp_project"
      (with-redefs [ui/do-run-later (fn [f] (f))]
        (let [lsp (lsp/get-node-lsp project)
              _ (lsp/set-servers!
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
                        "exit" (constantly nil)})}
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
                        "exit" (constantly nil)})}
                    ;; no lint at all
                    {:languages #{"json"}
                     :launcher
                     (make-test-server-launcher
                       {"initialize" (constantly {:capabilities {:textDocumentSync lsp.server/lsp-text-document-sync-kind-none}})
                        "initialized" (constantly nil)
                        "shutdown" (constantly nil)
                        "exit" (constantly nil)})}})
              _ (Thread/sleep 100)
              ret (promise)
              _ (lsp/pull-workspace-diagnostics! lsp ret)]
          (is
            (= {(workspace/find-resource workspace "/foo.json")
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
               @ret))))))
  (testing "Failing server does not block workspace diagnostics"
    (tu/with-loaded-project "test/resources/lsp_project"
      (with-redefs [ui/do-run-later (fn [f] (f))]
        (let [lsp (lsp/get-node-lsp project)
              _ (lsp/set-servers!
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
                        "exit" (constantly nil)})}
                    ;; broken: fails on diagnostic request
                    {:languages #{"json"}
                     :launcher
                     (make-test-server-launcher
                       {"initialize" (constantly {:capabilities {:diagnosticProvider {:workspaceDiagnostics true}}})
                        "initialized" (constantly nil)
                        "workspace/diagnostic" (fn [_ _] (throw (ex-info "Fail!" {})))
                        "shutdown" (constantly nil)
                        "exit" (constantly nil)})}})
              _ (Thread/sleep 100)
              ret (promise)
              _ (lsp/pull-workspace-diagnostics! lsp ret)]
          (is (= {(workspace/find-resource workspace "/foo.json")
                  (sorted-set (data/map->CursorRange
                                {:from (data/->Cursor 0 0)
                                 :to (data/->Cursor 0 1)
                                 :message "It's a bad start!"
                                 :severity :error}))}
                 @ret))))))
  (testing "the LSP client only waits up to a timeout"
    (tu/with-loaded-project "test/resources/lsp_project"
      (with-redefs [ui/do-run-later (fn [f] (f))]
        (let [lsp (lsp/get-node-lsp project)
              _ (lsp/set-servers!
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
                                  "exit" (constantly nil)})}})
              _ (Thread/sleep 100)
              ret (promise)
              _ (lsp/pull-workspace-diagnostics! lsp ret :timeout-ms 500)]
          (is (nil? @ret)))))))
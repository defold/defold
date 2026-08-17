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
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.code.data :as data]
            [editor.defold-project :as project]
            [editor.lsp :as lsp]
            [editor.lsp.base :as lsp.base]
            [editor.lsp.jsonrpc :as lsp.jsonrpc]
            [editor.lsp.project :as lsp.project]
            [editor.lsp.server :as lsp.server]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.util :as util]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.async-support :as async-support]
            [support.test-support :as test-support]
            [util.coll :as coll]
            [util.path :as path])
  (:import [java.io PipedInputStream PipedOutputStream]))

(set! *warn-on-reflection* true)

(def ^:private await-lsp-settle-rounds 5)
(def ^:private await-lsp-settle-sleep-ms 20)

(defmacro await-lsp [lsp & forms]
  `(let [lsp# ~lsp
         run-laters# (atom [])]
     (with-redefs [ui/do-run-later (fn [run-later-fn#]
                                     (swap! run-laters# conj run-later-fn#))]
       (let [result# (do ~@forms)]
         (loop [remaining-settle-rounds# await-lsp-settle-rounds]
           (lsp/await lsp#)
           (Thread/sleep (unchecked-long await-lsp-settle-sleep-ms))
           (let [pending-run-laters# @run-laters#]
             (reset! run-laters# [])
             (cond
               (not (coll/empty? pending-run-laters#))
               (do
                 (run! #(%)
                       pending-run-laters#)
                 (recur await-lsp-settle-rounds))

               (pos? remaining-settle-rounds#)
               (recur (dec remaining-settle-rounds#))

               :else
               result#)))))))

(defmacro with-scratch-project [project-path & forms]
  `(test-util/with-scratch-project ~project-path
     (try
       ~@forms
       (finally
         (let [lsp# (lsp/get-node-lsp ~'project)]
           (await-lsp lsp#
             (lsp/set-servers! lsp# #{})))))))

(defn- set-servers! [lsp new-servers]
  (await-lsp lsp
    (lsp/set-servers! lsp new-servers)))

(defn- open-view! [lsp view-node resource lines]
  (await-lsp lsp
    (lsp/open-view! lsp view-node resource lines)))

(defn- close-view! [lsp view-node]
  (await-lsp lsp
    (lsp/close-view! lsp view-node)))

(defn- edit-file! [lsp code-resource-node-id lines-or-text]
  (await-lsp lsp
    (test-util/set-code-editor-source! code-resource-node-id lines-or-text)))

(defn- rename-file! [lsp resource new-file-name]
  (await-lsp lsp
    (asset-browser/rename resource new-file-name test-util/localization)))

(defn- delete-file! [lsp resource]
  (await-lsp lsp
    (asset-browser/delete [resource])))

(defn- resource-sync! [lsp workspace]
  (await-lsp lsp
    (workspace/resource-sync! workspace)))

(defn- handler-run! [command lsp project]
  (await-lsp lsp
    (test-util/handler-run command [{:name :global :env {:project-graph (project/graph project)}}] {})))

(def ^:private undo! (partial handler-run! :edit.undo))

(def ^:private redo! (partial handler-run! :edit.redo))

(defn- pull-diagnostics! [lsp & args]
  (await-lsp lsp
    (let [ret (promise)]
      (apply lsp/pull-workspace-diagnostics! lsp ret args)
      @ret)))

(defn- hover! [lsp resource cursor]
  (await-lsp lsp
    (let [ret (promise)]
      (lsp/hover! lsp resource cursor ret)
      @ret)))

(defn- prepare-rename [lsp resource cursor]
  (await-lsp lsp
    (let [ret (promise)]
      (lsp/prepare-rename lsp resource cursor ret)
      @ret)))

(defn- rename [lsp prepared-range new-name]
  (await-lsp lsp
    (let [ret (promise)]
      (lsp/rename lsp prepared-range new-name ret)
      @ret)))

(defn- await-until [pred]
  (async-support/eventually
    (a/go-loop []
      (if-let [value (pred)]
        value
        (do
          (<! (a/timeout 10))
          (recur))))))

(defn- await-value= [expected f]
  (await-until
    (fn []
      (let [actual (f)]
        (when (= expected actual)
          actual)))))

(defmacro await= [expected actual-expr]
  `(let [expected# ~expected]
     (await-until
       (fn []
         (= expected# ~actual-expr)))))

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

(deftest lua-configuration-includes-official-annotations-test
  (with-scratch-project "test/resources/lsp_project"
    (let [official-annotations-path (str (path/of "/defold"
                                                  "shared"
                                                  "lua-annotations"))
          lua-language-server-plugin-path (str (path/of "/defold"
                                                        "shared"
                                                        "lua-language-server"
                                                        "plugin.lua"))
          configuration-handler (#'lsp.server/configuration-handler project)
          [configuration] (with-redefs [system/defold-unpack-path (constantly "/defold")]
                            (configuration-handler {:items [{:section "Lua"}]}))
          libraries (get-in configuration [:workspace :library])]
      (is (= lua-language-server-plugin-path (get-in configuration [:runtime :plugin])))
      (is (= official-annotations-path (first libraries)))
      (is (= 2 (count libraries)))
      (is (string/ends-with? (second libraries)
                             (str java.io.File/separator
                                  ".internal"
                                  java.io.File/separator
                                  "lua-annotations"))))))

(deftest lua-language-server-only-trusts-bundled-plugin-test
  (let [trust-action {:title "Trust and load this plugin\n"}
        reject-action {:title "Don't load this plugin\n"}
        bundled-plugin-path (str (path/of "/defold"
                                          "shared"
                                          "lua-language-server"
                                          "plugin.lua"))
        request {:actions [trust-action reject-action]
                 :message (str "The current settings try to load the plugin at this location:"
                               bundled-plugin-path
                               "\n\nNote that malicious plugin may harm your computer\n")}
        handler #'lsp.server/lua-language-server-show-message-request-handler]
    (with-redefs [system/defold-unpack-path (constantly "/defold")]
      (is (= trust-action (handler request)))
      (is (= trust-action (handler (assoc request :actions [reject-action trust-action]))))
      (is (nil? (handler (assoc request :actions [reject-action]))))
      (is (nil? (handler (update request :message string/replace bundled-plugin-path "/tmp/plugin.lua")))))))

(deftest lsp-server-test
  (testing "Initialize + open text document -> should publish diagnostics"
    (with-scratch-project "test/resources/lsp_project"
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
                  :find-references false
                  :document-symbol false
                  :hover false
                  :rename false}]
                [:on-publish-diagnostics
                 (test-util/resource workspace "/foo.json")
                 {:items [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                            :message "It's a bad start!" :severity :error)]}]]
               (async-support/eventually
                 (a/go
                   (>! in (lsp.server/open-text-document (test-util/resource workspace "/foo.json") foo-json-lines))
                   (<! (a/timeout 10))
                   (a/close! in)
                   (<! (a/reduce conj [] out))))))))))

(deftest project-completions-test
  (with-scratch-project "test/resources/project_lsp_completion_project"
    (let [lsp (lsp/get-node-lsp project)
          completion-result!
          (fn [proj-path line-prefix]
            (let [resource (test-util/resource workspace proj-path)
                  lines (g/node-value (project/get-resource-node project resource) :lines)
                  row (coll/first-index-where #(string/starts-with? % line-prefix) lines)]
              (assert row (str "Line prefix not found in " proj-path ": " line-prefix))
              (await-lsp lsp
                (let [ret (promise)]
                  (lsp/request-completions!
                    lsp
                    resource
                    (data/->Cursor row (count line-prefix))
                    {:trigger-kind :invoked}
                    ret)
                  @ret))))

          completion-items!
          (fn [proj-path line-prefix]
            (:items (completion-result! proj-path line-prefix)))

          completion-labels!
          (fn [proj-path line-prefix]
            (coll/into-> (completion-items! proj-path line-prefix) #{}
              (map :display-string)))

          main-urls #{"/enemy"
                      "/enemy#enemy-controller"
                      "/enemy#enemy-visual"
                      "/level/boss"
                      "/level/boss#boss-controller"
                      "/level/boss#boss-visual"
                      "/level/decoy-id"
                      "/level/decoy-id#decoy-component"
                      "/player"
                      "/player#controller"
                      "/player#player-visual"
                      "/readonly"
                      "/readonly#readonly-controller"}
          other-urls #{"/other-player"
                       "/other-player#controller"
                       "/other-player#player-visual"
                       "/spectator"
                       "/spectator#spectator-component"}
          level-urls #{"/boss"
                       "/boss#boss-controller"
                       "/boss#boss-visual"
                       "/decoy-id"
                       "/decoy-id#decoy-component"}
          player-urls (coll/into-> other-urls main-urls)
          shared-module-urls (coll/into-> level-urls player-urls)]
      (set-servers! lsp #{(lsp.project/language-server project)})

      (testing "Component ids in scripts"
        (is (false? (:complete (completion-result! "/scripts/player.script" "local hash_double = \"#"))))
        (is (= #{"controller" "player-visual"}
               (completion-labels! "/scripts/player.script" "local hash_double = \"#")))
        (is (= #{"controller" "player-visual"}
               (completion-labels! "/scripts/player.script" "local hash_single = '#")))
        (is (= ["controller" "player-visual"]
               (mapv :display-string
                     (completion-items! "/scripts/player.script" "local hash_double = \"#"))))
        (let [completion (coll/first-where
                           #(= "player-visual" (:display-string %))
                           (completion-items! "/scripts/player.script" "local hash_partial = \"#player-v"))]
          (is (= {:value "player-visual"
                  :cursor-range (data/->CursorRange (data/->Cursor 19 23) (data/->Cursor 19 31))
                  :type :plaintext}
                 (:insert completion))))
        (is (= #{"controller" "player-visual"}
               (completion-labels! "/scripts/player.script" "local hash_punctuation = \"#player."))))

      (testing "Absolute urls in scripts"
        (is (= player-urls
               (completion-labels! "/scripts/player.script" "local path_double = \"/pla")))
        (is (= player-urls
               (completion-labels! "/scripts/player.script" "local path_single = '/pla")))
        (is (= (coll/into-> (coll/sort util/natural-order player-urls) [])
               (mapv :display-string
                     (completion-items! "/scripts/player.script" "local path_double = \"/pla"))))
        (let [completion (coll/first-where
                           #(= "/player" (:display-string %))
                           (completion-items! "/scripts/player.script" "local path_double = \"/pla"))]
          (is (= {:value "/player"
                  :cursor-range (data/->CursorRange (data/->Cursor 4 21) (data/->Cursor 4 25))
                  :type :plaintext}
                 (:insert completion)))))

      (testing "Socket urls in scripts"
        (let [socket-urls (coll/into-> player-urls #{}
                            (map #(str "world:" %)))]
          (is (= socket-urls
                 (completion-labels! "/scripts/player.script" "local socket_double = \"world:/pla")))
          (is (= socket-urls
                 (completion-labels! "/scripts/player.script" "local socket_single = 'world:/pla")))
          (is (= socket-urls
                 (completion-labels! "/scripts/player.script" "local socket_empty_double = \"world:")))
          (is (= socket-urls
                 (completion-labels! "/scripts/player.script" "local socket_empty_single = 'world:")))
          (is (= (coll/into-> (coll/sort util/natural-order socket-urls) [])
                 (mapv :display-string
                       (completion-items! "/scripts/player.script" "local socket_double = \"world:/pla"))))
          (is (= (coll/into-> player-urls #{}
                   (map #(str "1-world.:" %)))
                 (completion-labels! "/scripts/player.script" "local socket_punctuation = \"1-world.:"))))
        (let [completion (coll/first-where
                           #(= "world:/player" (:display-string %))
                           (completion-items! "/scripts/player.script" "local socket_double = \"world:/pla"))]
          (is (= {:value "world:/player"
                  :cursor-range (data/->CursorRange (data/->Cursor 6 23) (data/->Cursor 6 33))
                  :type :plaintext}
                 (:insert completion))))
        (let [completion (coll/first-where
                           #(= "world:/player" (:display-string %))
                           (completion-items! "/scripts/player.script" "local socket_empty_double = \"world:"))]
          (is (= {:value "world:/player"
                  :cursor-range (data/->CursorRange (data/->Cursor 8 29) (data/->Cursor 8 35))
                  :type :plaintext}
                 (:insert completion)))))

      (testing "Direct and transitive Lua module contexts"
        (is (= player-urls
               (completion-labels! "/modules/direct.lua" "local path_double = \"/pla")))
        (is (= shared-module-urls
               (completion-labels! "/modules/shared.lua" "local path_double = \"/")))
        (is (= #{"boss-controller" "boss-visual" "controller" "enemy-controller" "enemy-visual" "player-visual"}
               (completion-labels! "/modules/shared.lua" "local hash_single = '#")))
        (is (= #{}
               (completion-labels! "/modules/unrequired.lua" "local path = \"/"))))

      (testing "Require calls"
        (is (= #{}
               (completion-labels! "/scripts/player.script" "local require_double = require(\"/pla")))
        (is (= #{}
               (completion-labels! "/scripts/player.script" "local require_single = require '/pla")))
        (is (= #{}
               (completion-labels! "/modules/direct.lua" "local require_double = require(\"/pla")))
        (is (= #{}
               (completion-labels! "/scripts/player.script" "local require_global = _G.require(\"/pla")))
        (is (= #{}
               (completion-labels! "/scripts/player.script" "local require_global_no_parens = _G.require '/pla")))
        (is (= #{}
               (completion-labels! "/scripts/player.script" "local divide_after_string = \"5\"/")))
        (is (= #{}
               (completion-labels! "/scripts/player.script" "local escaped_quote = \"\\\"/pla"))))

      (testing "Other Lua-backed resource types"
        (is (= #{}
               (completion-labels! "/scripts/ignored.gui_script" "local path = \"/pla")))
        (is (= #{}
               (completion-labels! "/scripts/ignored.render_script" "local path = \"/pla"))))

      (testing "Stale request position"
        (doseq [cursor [(data/->Cursor 0 1000)
                        (data/->Cursor 1000 0)]]
          (let [ret (promise)]
            (lsp/request-completions!
              lsp
              (test-util/resource workspace "/scripts/player.script")
              cursor
              {:trigger-kind :trigger-character
               :trigger-character "/"}
              ret)
            (is (= {:complete false
                    :items []}
                   @ret)))))

      (testing "Live require edits"
        (let [direct-script-node-id (project/get-resource-node project "/modules/direct.lua")]
          (edit-file! lsp direct-script-node-id
                      ["local shared = require(\"modules.unrequired\")"
                       "local path_double = \"/pla\""])
          (is (= (coll/into-> level-urls main-urls)
                 (completion-labels! "/modules/shared.lua" "local path_double = \"/")))
          (is (= player-urls
                 (completion-labels! "/modules/unrequired.lua" "local path = \"/")))

          (edit-file! lsp direct-script-node-id
                      ["local shared = require(\"modules.shared\")"
                       "local path_double = \"/pla\""])
          (is (= shared-module-urls
                 (completion-labels! "/modules/shared.lua" "local path_double = \"/"))))))))

(g/defnode LSPViewNode
  (property diagnostics g/Any (default []))
  (property document-symbols g/Any (default []))
  (property completion-trigger-characters g/Any (default #{})))

(defn- make-lsp-view-node! [app-view]
  (first
    (g/tx-nodes-added
      (g/transact
        {:undoable false}
        (g/make-node (g/node-id->graph-id app-view) LSPViewNode)))))

(deftest project-completion-trigger-characters-test
  (with-scratch-project "test/resources/project_lsp_completion_project"
    (let [lsp (lsp/get-node-lsp project)
          view-node (make-lsp-view-node! app-view)
          resource (test-util/resource workspace "/scripts/player.script")]
      (set-servers! lsp #{(lsp.project/language-server project)})
      (open-view! lsp view-node resource (g/node-value (project/get-resource-node project resource) :lines))
      (is (await= #{"#" "/" ":"} (g/node-value view-node :completion-trigger-characters)))
      (close-view! lsp view-node))))

(deftest start-open-order-test
  (with-scratch-project "test/resources/lsp_project"
    (let [lsp (lsp/get-node-lsp project)]
      (testing "Start server + open resource -> should receive diagnostics"
        (let [;; set servers
              _ (set-servers! lsp #{{:languages #{"json"}
                                     :launcher (make-test-server-launcher default-handlers)}})
              ;; open view
              view-node (make-lsp-view-node! app-view)
              _ (open-view! lsp view-node (test-util/resource workspace "/foo.json") foo-json-lines)]
          (is (await= [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
                         :type :diagnostic :hoverable true :messages ["It's a bad start!"] :severity :error)]
                      (g/node-value view-node :diagnostics)))
          (close-view! lsp view-node)))
      (testing "Open resource + start server -> should receive diagnostics"
        (let [;; open view
              view-node (make-lsp-view-node! app-view)
              _ (open-view! lsp view-node (test-util/resource workspace "/foo.json") foo-json-lines)
              ;; set servers
              _ (set-servers! lsp #{{:languages #{"json"}
                                     :launcher (make-test-server-launcher default-handlers)}})]
          (is (await= [(assoc (data/->CursorRange (data/->Cursor 0 0) (data/->Cursor 0 1))
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
      (with-scratch-project "test/resources/lsp_project"
        (let [lsp (lsp/get-node-lsp project)
              _ (set-servers!
                  lsp
                  #{{:languages #{"json"}
                     :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-incremental))}
                    {:languages #{"json"}
                     :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-full))}
                    {:languages #{"json"}
                     :launcher (make-test-server-launcher (make-handlers lsp.server/lsp-text-document-sync-kind-none))}})
              view-node (make-lsp-view-node! app-view)
              foo-resource (test-util/resource workspace "/foo.json")
              foo-node (test-util/resource-node project "/foo.json")
              lines (g/node-value foo-node :lines)
              _ (open-view! lsp view-node (test-util/resource workspace "/foo.json") lines)
              _ (edit-file!
                  lsp
                  foo-node
                  (data/splice-lines lines {data/document-start-cursor-range ["NEWTEXT"]}))]
          (is (await= #{[lsp.server/lsp-text-document-sync-kind-incremental
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
          (edit-file! lsp foo-node lines)
          (close-view! lsp view-node))))))

(deftest polled-resources-test
  (testing "Modifying resources without any views should make the language servers open the document anyway"
    (with-scratch-project "test/resources/lsp_project"
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
            _ (edit-file! lsp foo-node "{}")
            _ (is (await= #{(lsp.server/resource-uri foo-resource)}
                          @server-opened-docs))

            ;; modify to initial => clean
            _ (edit-file! lsp foo-node initial-source)
            _ (is (await= #{} @server-opened-docs))

            ;; undo => dirty again
            _ (undo! lsp project)
            _ (is (await= #{(lsp.server/resource-uri foo-resource)}
                          @server-opened-docs))

            ;; redo => clean again
            _ (redo! lsp project)
            _ (is (await= #{} @server-opened-docs))]))))

(deftest open-close-test
  (with-scratch-project "test/resources/lsp_project"
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
              view-node (make-lsp-view-node! app-view)
              foo-resource (test-util/resource workspace "/foo.json")
              foo-resource-uri (lsp.server/resource-uri foo-resource)
              ;; open view
              _ (open-view! lsp view-node foo-resource foo-json-lines)
              _ (is (await= #{foo-resource-uri} @server-opened-docs))
              ;; close view
              _ (close-view! lsp view-node)
              _ (is (await= #{} @server-opened-docs))]))
      (testing "Open view -> notify open, modify lines + close view -> still open"
        (let [_ (set-servers!
                  lsp
                  #{{:languages #{"json"}
                     :launcher (make-test-server-launcher handlers)}})
              view-node (make-lsp-view-node! app-view)
              foo-resource (test-util/resource workspace "/foo.json")
              foo-resource-node (test-util/resource-node project "/foo.json")
              foo-resource-uri (lsp.server/resource-uri foo-resource)
              ;; open view
              _ (open-view! lsp view-node foo-resource foo-json-lines)
              _ (is (await= #{foo-resource-uri} @server-opened-docs))
              ;; modify lines, close view
              _ (edit-file! lsp foo-resource-node "{}")
              _ (close-view! lsp view-node)
              _ (is (await= #{foo-resource-uri} @server-opened-docs))])))))

(deftest resource-changes-test
  (testing "Modify lines -> notify open, rename file -> close + open modified"
    (with-scratch-project "test/resources/lsp_project"
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
            _ (edit-file! lsp foo-resource-node "{}")
            _ (is (await= {foo-resource-uri "{}"} @server-opened-docs))

            ;; rename foo.json to bar.json
            _ (rename-file! lsp [foo-resource] "bar")
            bar-resource (test-util/resource workspace "/bar.json")
            bar-resource-uri (lsp.server/resource-uri bar-resource)
            _ (is (await= {bar-resource-uri "{}"} @server-opened-docs))]
        (edit-file! lsp (test-util/resource-node project "/bar.json") old-foo-content)
        (rename-file! lsp [bar-resource] "foo"))))
  (testing "Open view -> notify open, change on disk + resource sync -> notify changed"
    (with-scratch-project "test/resources/lsp_project"
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
            view-node (make-lsp-view-node! app-view)
            foo-resource (test-util/resource workspace "/foo.json")
            old-foo-content (slurp foo-resource)
            foo-resource-node (test-util/resource-node project "/foo.json")
            foo-resource-uri (lsp.server/resource-uri foo-resource)
            lines (g/node-value foo-resource-node :lines)
            ;; open view
            _ (open-view! lsp view-node foo-resource lines)
            _ (is (await= #{foo-resource-uri} @server-opened-docs))
            ;; modify on disk + sync
            _ (test-support/spit-until-new-mtime foo-resource "NEW_CONTENT")
            _ (resource-sync! lsp workspace)
            _ (is (await= [{:textDocument {:uri foo-resource-uri :version 1}
                            :contentChanges [{:text "NEW_CONTENT"}]}]
                          @change-notifications))]
        (test-support/spit-until-new-mtime foo-resource old-foo-content)
        (resource-sync! lsp workspace)
        (close-view! lsp view-node))))
  (testing "Modify lines -> notify open; delete file + sync -> notify closed"
    (with-scratch-project "test/resources/lsp_project"
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
            _ (edit-file! lsp foo-resource-node "{}")
            _ (is (await= #{foo-resource-uri} @server-opened-docs))
            ;; delete file
            _ (delete-file! lsp foo-resource)
            _ (is (await= #{} @server-opened-docs))]
        (test-support/spit-until-new-mtime foo-resource old-foo-content)
        (resource-sync! lsp workspace)))))

(deftest workspace-diagnostics-test
  (testing "Workspace diagnostics with different pull diagnostics kinds"
    (with-scratch-project "test/resources/lsp_project"
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
        (is (await= {(workspace/find-resource workspace "/foo.json")
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
        (set-servers! lsp #{})
        (is (true? (deref workspace-lint-exit-promise 200 false)))
        (is (true? (deref document-lint-exit-promise 200 false)))
        (is (true? (deref no-lint-exit-promise 200 false))))))
  (testing "Failing server does not block workspace diagnostics"
    (with-scratch-project "test/resources/lsp_project"
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
        (is (await= {(workspace/find-resource workspace "/foo.json")
                     (sorted-set (data/map->CursorRange
                                   {:from (data/->Cursor 0 0)
                                    :to (data/->Cursor 0 1)
                                    :message "It's a bad start!"
                                    :severity :error}))}
                    (pull-diagnostics! lsp)))
        (set-servers! lsp #{})
        (is (true? (deref working-exit-promise 200 false)))
        (is (true? (deref broken-exit-promise 200 false))))))
  (testing "the LSP client only waits up to a timeout"
    (with-scratch-project "test/resources/lsp_project"
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
        (set-servers! lsp #{})
        (is (true? (deref exit-promise 1100 false)))))))

(deftest hover-test
  (with-scratch-project "test/resources/lsp_project"
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
      (is (await= [(data/map->CursorRange {:from (data/->Cursor 0 1)
                                           :to (data/->Cursor 0 2)
                                           :type :hover
                                           :hoverable true
                                           :content (lsp.server/->MarkupContent :markdown "hover")})]
                  (hover! lsp (test-util/resource workspace "/foo.json") (data/->Cursor 0 1))))
      (is (true? (await-until #(when (realized? matched-promise) true))))
      (is (not (realized? unmatched-promise)))
      (set-servers! lsp #{}))))

(deftest content-modified-errors-are-retried-test
  (with-scratch-project "test/resources/lsp_project"
    (let [hover-requests (atom 0)
          lsp (lsp/get-node-lsp project)
          _ (set-servers! lsp #{{:languages #{"json"}
                                 :launcher (make-test-server-launcher
                                             {"initialize" (constantly {:capabilities {:hoverProvider true}})
                                              "initialized" (constantly nil)
                                              "shutdown" (constantly nil)
                                              "textDocument/hover" (fn [_ _]
                                                                     (if (= 1 (swap! hover-requests inc))
                                                                       (throw (ex-info "Content modified." {:jsonrpc/code -32801}))
                                                                       {:contents {:kind :markdown :value "hover"}}))
                                              "exit" (constantly nil)})}})]
      (is (await= [(data/map->CursorRange {:from (data/->Cursor 0 1)
                                           :to (data/->Cursor 0 2)
                                           :type :hover
                                           :hoverable true
                                           :content (lsp.server/->MarkupContent :markdown "hover")})]
                  (hover! lsp (test-util/resource workspace "/foo.json") (data/->Cursor 0 1))))
      (is (await= 2 @hover-requests))
      (set-servers! lsp #{}))))

(deftest rename-test
  (with-scratch-project "test/resources/lsp_project"
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
                                              "exit" (constantly nil)})}})
          resource (test-util/resource workspace "/foo.json")
          rename-region (await-value=
                          #code/range[[0 0] [0 1]]
                          #(prepare-rename lsp resource (data/->Cursor 0 0)))]
      (is (= #code/range[[0 0] [0 1]] rename-region))
      (is (await= {resource [[#code/range [[0 0] [0 1]] ["foo"]]]}
                  (rename lsp rename-region "foo")))
      (set-servers! lsp #{}))))

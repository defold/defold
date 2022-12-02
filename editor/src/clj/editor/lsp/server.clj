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

(ns editor.lsp.server
  (:require [clojure.core.async :as a :refer [<! >!]]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.lsp.async :as lsp.async]
            [editor.lsp.base :as lsp.base]
            [editor.lsp.jsonrpc :as lsp.jsonrpc]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [editor.code.data Cursor CursorRange]
           [java.io InputStream]
           [java.lang ProcessHandle]
           [java.net URI]
           [java.util Map]
           [java.util.concurrent TimeUnit]
           [java.util.function BiFunction]))

(set! *warn-on-reflection* true)

(defprotocol Connection
  (^InputStream input-stream [connection])
  (^InputStream output-stream [connection])
  (dispose [connection]))

(extend-protocol Connection
  Process
  (input-stream [process]
    (.getInputStream process))
  (output-stream [process]
    (.getOutputStream process))
  (dispose [process]
    (when (.isAlive process)
      (.destroy process)
      (-> process
          .onExit
          (.orTimeout 10 TimeUnit/SECONDS)
          (.handle (reify BiFunction
                     (apply [_ _ timeout]
                       (if timeout
                         (do (log/warn :message "Language server process didn't exit gracefully in time, terminating")
                             (.destroyForcibly process))
                         (when-not (zero? (.exitValue process))
                           (log/warn :message "Language server process exit code is not zero"
                                     :exit-code (.exitValue process)))))))))))

(defprotocol Launcher
  (launch [launcher]))

(extend-protocol Launcher
  Map
  (launch [{:keys [command]}]
    (.start (ProcessBuilder. ^"[Ljava.lang.String;" (into-array String command)))))

(defn- make-uri-string [abs-path]
  (str (URI. "file" "" abs-path nil)))

(defn resource-uri [resource]
  (make-uri-string (resource/abs-path resource)))

(defn- root-uri [workspace evaluation-context]
  (make-uri-string (g/node-value workspace :root evaluation-context)))

(defn- maybe-resource [project uri]
  (lsp.async/with-auto-evaluation-context evaluation-context
    (let [workspace (g/node-value project :workspace evaluation-context)]
      (when-let [proj-path (workspace/as-proj-path workspace (.getPath (URI. uri)) evaluation-context)]
        (workspace/find-resource workspace proj-path evaluation-context)))))

;; diagnostics
(s/def ::severity #{:error :warning :information :hint})
(s/def ::message string?)
(s/def ::cursor #(instance? Cursor %))
(s/def ::cursor-range #(instance? CursorRange %))
(s/def ::diagnostic (s/and ::cursor-range (s/keys :req-un [::severity ::message])))
(s/def ::diagnostics (s/coll-of ::diagnostic))

;; capabilities
(s/def ::open-close boolean?)
(s/def ::change #{:none :full :incremental})
(s/def ::text-document-sync (s/keys :req-un [::open-close ::change]))
(s/def ::capabilities (s/keys :req-un [::text-document-sync]))

(defn- lsp-position->editor-cursor [{:keys [line character]}]
  (data/->Cursor line character))

(defn- editor-cursor->lsp-position [{:keys [row col]}]
  {:line row
   :character col})

(defn- lsp-range->editor-cursor-range [{:keys [start end]}]
  (data/->CursorRange
    (lsp-position->editor-cursor start)
    (lsp-position->editor-cursor end)))

(defn- editor-cursor-range->lsp-range [{:keys [from to]}]
  {:start (editor-cursor->lsp-position from)
   :end (editor-cursor->lsp-position to)})

(defn- lsp-diagnostic->editor-diagnostic [{:keys [range message severity]}]
  (assoc (lsp-range->editor-cursor-range range)
    :message message
    :severity ({1 :error 2 :warning 3 :information 4 :hint} severity :error)))

(defn- diagnostics-handler [project out on-publish-diagnostics]
  (fn [{:keys [uri diagnostics]}]
    (when-let [resource (maybe-resource project uri)]
      (a/put! out (on-publish-diagnostics resource (mapv lsp-diagnostic->editor-diagnostic diagnostics))))))

(def lsp-text-document-sync-kind-incremental 2)
(def lsp-text-document-sync-kind-full 1)
(def lsp-text-document-sync-kind-none 0)

(defn- lsp-text-document-sync-kind->editor-sync-kind [n]
  (condp = n
    lsp-text-document-sync-kind-none :none
    lsp-text-document-sync-kind-full :full
    lsp-text-document-sync-kind-incremental :incremental))

(defn- lsp-capabilities->editor-capabilities [{:keys [textDocumentSync]}]
  {:text-document-sync (cond
                         (nil? textDocumentSync)
                         {:change :none :open-close false}

                         (number? textDocumentSync)
                         {:open-close true
                          :change (lsp-text-document-sync-kind->editor-sync-kind textDocumentSync)}

                         (map? textDocumentSync)
                         {:open-close (:openClose textDocumentSync false)
                          :change (lsp-text-document-sync-kind->editor-sync-kind (:change textDocumentSync 0))}

                         :else
                         (throw (ex-info "Invalid text document sync kind" {:value textDocumentSync})))})

(defn- initialize [jsonrpc project]
  (lsp.jsonrpc/request!
    jsonrpc
    "initialize"
    (lsp.async/with-auto-evaluation-context evaluation-context
      (let [uri (root-uri (g/node-value project :workspace evaluation-context) evaluation-context)
            title ((g/node-value project :settings evaluation-context) ["project" "title"])]
        {:processId (.pid (ProcessHandle/current))
         :rootUri uri
         :capabilities {:workspace {:diagnostics {}}}
         :workspaceFolders [{:uri uri :name title}]}))
    (* 60 1000)))

(defn make
  "Start language server process and perform its lifecycle as defined by LSP

  The process will perform server initialization, then it will execute all
  server actions received from in until the channel closes, then it will shut
  the server down. The output channel will close when the server shuts down,
  either because it was requested or due to an error.

  Required args:
    project     defold project node id
    launcher    the server launcher, e.g. a map with following keys:
                  :command    a shell command to launch the language process,
                              vector of strings, required
    in          input channel that server will take items from to execute,
                items are server actions created by other public fns in this ns
    out         output channel that server can submit values to, valid values
                are those produced by calling kv-arg callbacks; will be closed
                when the server shuts down (either because it was requested or
                due to an error)

  Required kv-args:
    :on-publish-diagnostics    a function of 2 args, resource and diagnostics
                               vector, produces a value that can be submitted to
                               out channel to notify the process higher in the
                               hierarchy about published diagnostics
    :on-initialized            a function of 1 arg, a server capabilities map,
                               produces a value for out channel

  Returns a channel that will close when the LSP server closes.

  See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#lifeCycleMessages"
  [project launcher in out & {:keys [on-publish-diagnostics
                                     on-initialized]}]
  (a/go
    (try
      (let [connection (<! (a/thread (try (launch launcher) (catch Throwable e e))))]
        (if (instance? Throwable connection)
          (log/error :message "Language server process failed to start"
                     :launcher launcher
                     :exception connection)
          (try
            (let [[base-source base-sink base-err] (lsp.base/make (input-stream connection) (output-stream connection))
                  jsonrpc (lsp.jsonrpc/make
                            {"textDocument/publishDiagnostics" (diagnostics-handler project out on-publish-diagnostics)}
                            base-source
                            base-sink)]
              (a/alt!
                (a/go
                  (try
                    ;; LSP lifecycle: initialization
                    (let [{:keys [capabilities]} (lsp.jsonrpc/unwrap-response (<! (initialize jsonrpc project)))]
                      (>! out (on-initialized (lsp-capabilities->editor-capabilities capabilities)))
                      (>! jsonrpc (lsp.jsonrpc/notification "initialized"))
                      ;; LSP lifecycle: serve requests
                      (<! (lsp.async/pipe in jsonrpc false))
                      ;; LSP lifecycle: shutdown
                      (lsp.jsonrpc/unwrap-response (<! (lsp.jsonrpc/request! jsonrpc "shutdown" (* 10 1000))))
                      (>! jsonrpc (lsp.jsonrpc/notification "exit"))
                      (a/close! jsonrpc)
                      ;; give server a bit of time to exit
                      (<! (a/timeout 100)))
                    nil
                    (catch Throwable e e)))
                ([maybe-e]
                 (when maybe-e (throw maybe-e)))

                base-err
                ([e] (throw e))))

            (catch Throwable e
              (log/warn :message "Language server failed"
                        :launcher launcher
                        :exception e))
            (finally
              (when-let [e (<! (a/thread (try (dispose connection) nil (catch Throwable e e))))]
                (log/warn :message "Failed to dispose language server"
                          :launcher launcher
                          :exception e))))))
      (finally
        (a/close! out)))))

(defn open-text-document
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didOpen"
  [resource lines]
  (lsp.jsonrpc/notification
    "textDocument/didOpen"
    {:textDocument {:uri (resource-uri resource)
                    :languageId (resource/language resource)
                    :version 0
                    :text (slurp (data/lines-reader lines))}}))

(defn close-text-document
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didClose"
  [resource]
  (lsp.jsonrpc/notification
    "textDocument/didClose"
    {:textDocument {:uri (resource-uri resource)}}))

(defn full-text-document-change
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didChange"
  [resource lines version]
  (lsp.jsonrpc/notification
    "textDocument/didChange"
    {:textDocument {:uri (resource-uri resource)
                    :version version}
     :contentChanges [{:text (slurp (data/lines-reader lines))}]}))

(defn incremental-document-change
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_didChange"
  [resource incremental-diff version]
  (lsp.jsonrpc/notification
    "textDocument/didChange"
    {:textDocument {:uri (resource-uri resource)
                    :version version}
     :contentChanges (into []
                           (map (fn [[cursor-range replacement]]
                                  {:range (editor-cursor-range->lsp-range cursor-range)
                                   :text (string/join "\n" replacement)}))
                           incremental-diff)}))

(defn watched-file-change
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#workspace_didChangeWatchedFiles"
  [resource+change-types]
  (lsp.jsonrpc/notification
    "workspace/didChangeWatchedFiles"
    {:changes (mapv (fn [[resource change-type]]
                      {:uri (resource-uri resource)
                       :type (case change-type
                               :created 1
                               :changed 2
                               :deleted 3)})
                    resource+change-types)}))
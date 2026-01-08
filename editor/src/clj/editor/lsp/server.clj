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

(ns editor.lsp.server
  (:require [clojure.core.async :as a :refer [<! >!]]
            [clojure.java.io :as io]
            [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code-completion :as code-completion]
            [editor.code.data :as data]
            [editor.code.util :as util]
            [editor.fs :as fs]
            [editor.lsp.async :as lsp.async]
            [editor.lsp.base :as lsp.base]
            [editor.lsp.jsonrpc :as lsp.jsonrpc]
            [editor.lua :as lua]
            [editor.os :as os]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [service.log :as log]
            [util.coll :as coll]
            [util.eduction :as e])
  (:import [editor.code.data Cursor CursorRange]
           [java.io File InputStream]
           [java.lang ProcessBuilder$Redirect ProcessHandle]
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
  (launch [launcher ^File directory]))

(extend-protocol Launcher
  Map
  (launch [{:keys [command]} ^File directory]
    {:pre [(vector? command)]}
    ;; We need to resolve command in addition to setting ProcessBuilder's directory,
    ;; since the latter only affects the current directory of the spawned process,
    ;; and does not affect executable resolution
    (let [resolved-command (update command 0 #(str (.resolve (.toPath directory) (.toPath (io/file %)))))]
      (.start
        (doto (ProcessBuilder. ^"[Ljava.lang.String;" (into-array String resolved-command))
          (.redirectError ProcessBuilder$Redirect/INHERIT)
          (.directory directory))))))

(defprotocol Message
  (->jsonrpc [input project on-response]))

(extend-protocol Message
  Object
  (->jsonrpc [input _ _] input)
  nil
  (->jsonrpc [input _ _] input))

(deftype RawRequest [notification result-converter]
  Message
  (->jsonrpc [this _ _]
    (throw (ex-info "Can't send raw request: use finalize-request first" {:request this}))))

(defn finalize-request [^RawRequest raw-request id]
  (reify Message
    (->jsonrpc [_ project on-response]
      (let [ch (a/chan 1)
            result-converter (.-result-converter raw-request)
            request (lsp.jsonrpc/notification->request (.-notification raw-request) ch)]
        (a/take! ch (fn [response]
                      (on-response
                        id
                        (cond-> response (not (:error response)) (update :result result-converter project)))))
        request))))

(defn- raw-request [notification result-converter]
  (->RawRequest notification result-converter))

(defn- make-uri-string [abs-path]
  (let [path (if (os/is-win32?)
               (str "/" (string/replace abs-path "\\" "/"))
               abs-path)]
    (str (URI. "file" "" path nil))))

(defn resource-uri [resource]
  (make-uri-string (resource/abs-path resource)))

(defn- root-uri [basis workspace]
  (make-uri-string (g/raw-property-value basis workspace :root)))

(defn- maybe-resource [project uri evaluation-context]
  (let [basis (:basis evaluation-context)
        workspace (g/node-value project :workspace evaluation-context)]
    (when-let [proj-path (workspace/as-proj-path basis workspace (.getPath (URI. uri)))]
      (workspace/find-resource workspace proj-path evaluation-context))))

;; diagnostics
(s/def ::severity #{:error :warning :information :hint})
(s/def ::message string?)
(s/def ::cursor #(instance? Cursor %))
(s/def ::cursor-range #(instance? CursorRange %))
(s/def ::diagnostic (s/and ::cursor-range (s/keys :req-un [::severity ::message])))
(s/def ::result-id string?)
(s/def ::version int?)
(s/def ::items (s/coll-of ::diagnostic))
(s/def ::diagnostics-result (s/keys :req-un [::items]
                                    :opt-un [::result-id
                                             ::version]))

;; capabilities
(s/def ::open-close boolean?)
(s/def ::change #{:none :full :incremental})
(s/def ::text-document-sync (s/keys :req-un [::open-close ::change]))
(s/def ::pull-diagnostics #{:none :text-document :workspace})
(s/def ::goto-definition boolean?)
(s/def ::find-references boolean?)
(s/def ::resolve boolean?) ;; if completion item can be resolved to add e.g. documentation
(s/def ::trigger-characters (s/coll-of string? :kind set?))
(s/def ::completion (s/keys :req-un [::resolve ::trigger-characters]))
(s/def ::hover boolean?)
(s/def ::rename boolean?)
(s/def ::capabilities (s/keys :req-un [::text-document-sync
                                       ::pull-diagnostics
                                       ::goto-definition
                                       ::find-references
                                       ::hover
                                       ::rename]
                              :opt-un [::completion]))

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

(defn- lsp-diagnostic-result->editor-diagnostic-result
  [{:keys [resultId version] :as lsp-diagnostics-result} diagnostics-key]
  (cond-> {:items (mapv lsp-diagnostic->editor-diagnostic (get lsp-diagnostics-result diagnostics-key))}
          resultId
          (assoc :result-id resultId)
          version
          (assoc :version version)))

(defn- diagnostics-handler [project out on-publish-diagnostics]
  (fn [{:keys [uri] :as result}]
    (lsp.async/with-auto-evaluation-context evaluation-context
      (when-let [resource (maybe-resource project uri evaluation-context)]
        (let [result (lsp-diagnostic-result->editor-diagnostic-result result :diagnostics)]
          (a/put! out (on-publish-diagnostics resource result)))))))

(def lsp-text-document-sync-kind-incremental 2)
(def lsp-text-document-sync-kind-full 1)
(def lsp-text-document-sync-kind-none 0)

(defn- lsp-text-document-sync-kind->editor-sync-kind [n]
  (condp = n
    lsp-text-document-sync-kind-none :none
    lsp-text-document-sync-kind-full :full
    lsp-text-document-sync-kind-incremental :incremental))

(defn- lsp-capabilities->editor-capabilities [{:keys [textDocumentSync
                                                      diagnosticProvider
                                                      definitionProvider
                                                      referencesProvider
                                                      completionProvider
                                                      hoverProvider
                                                      renameProvider]}]
  (cond-> {:text-document-sync (cond
                                 (nil? textDocumentSync)
                                 {:change :none :open-close false}

                                 (number? textDocumentSync)
                                 {:open-close true
                                  :change (lsp-text-document-sync-kind->editor-sync-kind textDocumentSync)}

                                 (map? textDocumentSync)
                                 {:open-close (:openClose textDocumentSync false)
                                  :change (lsp-text-document-sync-kind->editor-sync-kind (:change textDocumentSync 0))}

                                 :else
                                 (throw (ex-info "Invalid text document sync kind" {:value textDocumentSync})))
           :pull-diagnostics (cond
                               (nil? diagnosticProvider) :none
                               (:workspaceDiagnostics diagnosticProvider) :workspace
                               :else :text-document)
           :goto-definition (if (boolean? definitionProvider)
                              definitionProvider
                              (map? definitionProvider))
           :find-references (if (boolean? referencesProvider)
                              referencesProvider
                              (map? referencesProvider))
           :hover (if (boolean? hoverProvider)
                    hoverProvider
                    (map? hoverProvider))
           :rename (and (map? renameProvider)
                        (let [prepare (:prepareProvider renameProvider)]
                          (and (boolean? prepare) prepare)))}
          completionProvider
          (assoc :completion {:resolve (boolean (:resolveProvider completionProvider))
                              :trigger-characters (set (:triggerCharacters completionProvider))})))

(defn- configuration-handler [project]
  (fn [{:keys [items]}]
    (lsp.async/with-auto-evaluation-context evaluation-context
      (mapv
        (fn [{:keys [section]}]
          (case section
            "Lua"
            (let [script-intelligence (g/node-value project :script-intelligence evaluation-context)
                  completions (g/node-value script-intelligence :lua-completions evaluation-context)
                  workspace (g/node-value project :workspace evaluation-context)
                  root (g/raw-property-value (:basis evaluation-context) workspace :root)]
              {:runtime {:version "Lua 5.1" :pathStrict true}
               :completion {:workspaceWord false}
               :diagnostics {:globals (-> lua/defined-globals
                                          (into (lua/extract-globals-from-completions completions))
                                          (into (lua/extract-globals-from-completions lua/editor-completions)))}
               :workspace {:library [(str (fs/path root ".internal" "lua-annotations"))]}})

            "files.associations"
            (let [workspace (g/node-value project :workspace evaluation-context)
                  resource-types (g/node-value workspace :resource-types evaluation-context)]
              (into {}
                    (keep (fn [[ext {:keys [textual? language]}]]
                            (when (and textual? (not= "plaintext" language))
                              [(str "*." ext) language])))
                    resource-types))

            "files.exclude"
            ["/build" "/.internal"]

            nil))
        items))))

(def ^:private ^:const completion-item-tag-deprecated 1)
(def ^:private completion-item-tag:lsp->editor
  {completion-item-tag-deprecated :deprecated})

(def ^:private ^:const insert-text-mode-as-is 1)
(def ^:private insert-text-mode-adjust-indentation 2)

(def ^:private ^:const insert-text-format-plaintext 1)
(def ^:private ^:const insert-text-format-snippet 2)
(def ^:private insert-text-format:lsp->editor
  {insert-text-format-plaintext :plaintext
   insert-text-format-snippet :snippet})

(def ^:private completion-item-kind:lsp->editor
  {1 :text
   2 :method
   3 :function
   4 :constructor
   5 :field
   6 :variable
   7 :class
   8 :interface
   9 :module
   10 :property
   11 :unit
   12 :value
   13 :enum
   14 :keyword
   15 :snippet
   16 :color
   17 :file
   18 :reference
   19 :folder
   20 :enum-member
   21 :constant
   22 :struct
   23 :event
   24 :operator
   25 :type-parameter})

(defn- initialize [jsonrpc project]
  (lsp.jsonrpc/request!
    jsonrpc
    "initialize"
    (lsp.async/with-auto-evaluation-context evaluation-context
      (let [basis (:basis evaluation-context)
            uri (root-uri basis (g/node-value project :workspace evaluation-context))
            title ((g/node-value project :settings evaluation-context) ["project" "title"])]
        {:processId (.pid (ProcessHandle/current))
         :rootUri uri
         :capabilities {:workspace {:diagnostics {}
                                    :workspaceEdit {:documentChanges false
                                                    :normalizesLineEndings true}}
                        :textDocument {:definition {:dynamicRegistration false
                                                    :linkSupport true}
                                       :references {:dynamicRegistration false}
                                       :hover {:dynamicRegistration false
                                               :contentFormat [:markdown :plaintext]}
                                       :rename {:dynamicRegistration false
                                                :prepareSupport true
                                                :honorsChangeAnnotations false}}
                        :completion {:dynamicRegistration false
                                     :completionItem {:snippetSupport true
                                                      :commitCharactersSupport true
                                                      :documentationFormat ["markdown" "plaintext"]
                                                      :deprecatedSupport true
                                                      :preselectSupport false
                                                      :tagSupport {:valueSet [completion-item-tag-deprecated]}
                                                      :insertReplaceSupport false
                                                      :resolveSupport {:properties ["detail"
                                                                                    "documentation"
                                                                                    "additionalTextEdits"]}
                                                      :insertTextModeSupport {:valueSet [insert-text-mode-as-is
                                                                                         insert-text-mode-adjust-indentation]}
                                                      :labelDetailsSupport false}
                                     :completionItemKind {:valueSet (vec (sort (keys completion-item-kind:lsp->editor)))}
                                     :contextSupport true
                                     :insertTextMode insert-text-mode-adjust-indentation}}
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
    launcher    the server launcher, e.g. a map with the following keys:
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
    :on-response               a function of 2 args: request id and response,
                               produces a value for out channel

  Returns a channel that will close when the LSP server closes.

  See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#lifeCycleMessages"
  [project launcher in out & {:keys [on-publish-diagnostics
                                     on-initialized
                                     on-response]}]
  (a/go
    (try
      (let [directory (lsp.async/with-auto-evaluation-context evaluation-context
                        (let [basis (:basis evaluation-context)
                              workspace (g/node-value project :workspace evaluation-context)]
                          (workspace/project-directory basis workspace)))
            connection (<! (a/thread (try (launch launcher directory) (catch Throwable e e))))]
        (if (instance? Throwable connection)
          (log/error :message "Language server process failed to start"
                     :launcher launcher
                     :exception connection)
          (try
            (let [[base-source base-sink base-err] (lsp.base/make (input-stream connection) (output-stream connection))
                  jsonrpc (lsp.jsonrpc/make
                            {"textDocument/publishDiagnostics" (diagnostics-handler project out on-publish-diagnostics)
                             "workspace/diagnostic/refresh" (constantly nil)
                             "workspace/configuration" (configuration-handler project)
                             "window/showMessageRequest" (constantly nil)
                             "window/workDoneProgress/create" (constantly nil)}
                            base-source
                            base-sink)
                  on-response #(a/put! out (on-response %1 %2))]
              (a/alt!
                (a/go
                  (try
                    ;; LSP lifecycle: initialization
                    (let [{:keys [capabilities]} (lsp.jsonrpc/unwrap-response (<! (initialize jsonrpc project)))]
                      (>! out (on-initialized (lsp-capabilities->editor-capabilities capabilities)))
                      (>! jsonrpc (lsp.jsonrpc/notification "initialized"))
                      ;; LSP lifecycle: serve requests
                      (<! (lsp.async/pipe (a/map #(->jsonrpc % project on-response) [in]) jsonrpc false))
                      ;; LSP lifecycle: shutdown
                      (lsp.jsonrpc/unwrap-response (<! (lsp.jsonrpc/request! jsonrpc "shutdown" (* 10 1000))))
                      (>! jsonrpc (lsp.jsonrpc/notification "exit"))
                      (a/close! jsonrpc)
                      ;; give server some time to exit
                      (<! (a/timeout 5000)))
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

(defn- full-or-unchanged-diagnostic-result:lsp->editor [{:keys [kind] :as result}]
  (case kind
    "full" {:type :full
            :result (lsp-diagnostic-result->editor-diagnostic-result result :items)}
    "unchanged" {:type :unchanged
                 :result-id (:resultId result)}))

(defn pull-workspace-diagnostics [resource->previous-result-id result-converter]
  (raw-request
    (lsp.jsonrpc/notification
      "workspace/diagnostic"
      {:previousResultIds (into []
                                (map (fn [[resource previous-result-id]]
                                       {:uri (resource-uri resource)
                                        :value previous-result-id}))
                                resource->previous-result-id)})
    ;; bound-fn only needed for tests to pick up the test system
    (bound-fn convert-result [{:keys [items]} project]
      (result-converter
        (lsp.async/with-auto-evaluation-context evaluation-context
          (into {}
                (keep
                  (fn [{:keys [uri] :as item}]
                    (when-let [resource (maybe-resource project uri evaluation-context)]
                      [resource (full-or-unchanged-diagnostic-result:lsp->editor item)])))
                items))))))

(defn pull-document-diagnostics
  [resource previous-result-id result-converter]
  (raw-request
    (lsp.jsonrpc/notification
      "textDocument/diagnostic"
      (cond-> {:textDocument {:uri (resource-uri resource)}}
              previous-result-id
              (assoc :previousResultId previous-result-id)))
    (fn convert-result [result _]
      (result-converter (full-or-unchanged-diagnostic-result:lsp->editor result)))))

(defn- lsp-location-or-location-link->editor-location
  [location-or-location-link project evaluation-context]
  ;; Location: uri and range keys
  ;; LocationLink: targetUri and targetSelectionRange
  (let [lsp-uri (or (:uri location-or-location-link)
                    (:targetUri location-or-location-link))
        lsp-range (or (:range location-or-location-link)
                      (:targetSelectionRange location-or-location-link))]
    (when-let [resource (maybe-resource project lsp-uri evaluation-context)]
      {:resource resource
       :cursor-range (lsp-range->editor-cursor-range lsp-range)})))

(defn goto-definition
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_definition"
  [resource cursor]
  (raw-request
    (lsp.jsonrpc/notification
      "textDocument/definition"
      {:textDocument {:uri (resource-uri resource)}
       :position (editor-cursor->lsp-position cursor)})
    ;; bound-fn only needed for tests to pick up the test system
    (bound-fn [result project]
      (if (nil? result)
        []
        (lsp.async/with-auto-evaluation-context evaluation-context
          (if (map? result)
            (if-let [location (lsp-location-or-location-link->editor-location result project evaluation-context)]
              [location]
              [])
            (into []
                  (keep #(lsp-location-or-location-link->editor-location % project evaluation-context))
                  result)))))))

(defrecord MarkupContent [type value]
  ;; We need markup content to be Comparable since it ends up in cursor regions
  ;; that need to be comparable
  Comparable
  (compareTo [_ that]
    (let [ret (compare type (:type that))]
      (if (zero? ret)
        (compare value (:value that))
        ret))))

(defn- markup-content:lsp->editor [{:keys [kind value]}]
  {:pre [(string? value)]}
  (->MarkupContent
    (case kind
      "plaintext" :plaintext
      "markdown" :markdown)
    value))

(defn- text-edit:lsp->editor [{:keys [range newText]}]
  {:value newText
   :cursor-range (lsp-range->editor-cursor-range range)})

(defn- completion-item:lsp->editor
  [{:keys [label filterText insertText tags deprecated kind
           insertTextMode insertTextFormat commitCharacters
           detail documentation textEdit additionalTextEdits]
    :or {insertTextMode insert-text-mode-adjust-indentation
         insertTextFormat insert-text-format-plaintext}
    :as source}]
  (let [insert-text-format (insert-text-format:lsp->editor insertTextFormat)
        text-edit (some-> textEdit text-edit:lsp->editor)]
    (vary-meta
      (code-completion/make
        (or filterText label)
        :display-string label
        :insert (or (some-> text-edit (assoc :type insert-text-format))
                    {:type insert-text-format
                     :value (or insertText label)})
        :commit-characters (set commitCharacters)
        :type (some-> kind completion-item-kind:lsp->editor)
        :detail detail
        :doc (cond
               (string? documentation) (->MarkupContent :plaintext documentation)
               documentation (markup-content:lsp->editor documentation))
        :tags (into (if deprecated #{:deprecated} #{})
                    (map completion-item-tag:lsp->editor)
                    tags)
        :additional-edits (when (pos? (count additionalTextEdits))
                            (mapv text-edit:lsp->editor additionalTextEdits)))
      assoc ::completion source)))

(defn- completion-context:editor->lsp [{:keys [trigger-kind trigger-character]}]
  (cond-> {:triggerKind (case trigger-kind
                          :invoked 1
                          :trigger-character 2
                          :trigger-for-incomplete-completions 3)}
          trigger-character
          (assoc :triggerCharacter trigger-character)))

(defn completion
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_completion"
  [resource cursor context item-converter]
  (raw-request
    (lsp.jsonrpc/notification
      "textDocument/completion"
      {:textDocument {:uri (resource-uri resource)}
       :position (editor-cursor->lsp-position cursor)
       :context (completion-context:editor->lsp context)})
    (bound-fn [result _]
      (let [is-map (map? result)
            incomplete (if is-map (:isIncomplete result) false)
            items (if is-map (:items result) result)]
        {:complete (not incomplete)
         :items (mapv (comp item-converter completion-item:lsp->editor) items)}))))

(defn resolve-completion
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#completionItem_resolve"
  [completion item-converter]
  (raw-request
    (lsp.jsonrpc/notification "completionItem/resolve" (::completion (meta completion)))
    (bound-fn [result _]
      (item-converter
        (cond-> completion
                ;; LSP specification does not allow null completion resolution
                ;; results, but the Lua language server might return null anyway
                result
                (conj (select-keys (completion-item:lsp->editor result)
                                   [:detail :doc :additional-edits])))))))

(defn find-references
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_references"
  [resource cursor]
  (raw-request
    (lsp.jsonrpc/notification
      "textDocument/references"
      {:textDocument {:uri (resource-uri resource)}
       :position (editor-cursor->lsp-position cursor)
       :context {:includeDeclaration true}})
    (bound-fn [result project]
      (lsp.async/with-auto-evaluation-context evaluation-context
        (into []
              (keep (fn [{:keys [uri range]}]
                      (when-let [resource (maybe-resource project uri evaluation-context)]
                        {:resource resource
                         :cursor-range (lsp-range->editor-cursor-range range)})))
              result)))))

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

(defn- hover-response->region [range content]
  (assoc range
    :type :hover
    :hoverable true
    :content (cond
               (string? content) (->MarkupContent :plaintext content)
               (:language content) (->MarkupContent :markdown (str "```" (:language content) "\n" (:value content) "\n```"))
               :else (markup-content:lsp->editor content))))

(defn hover
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_hover"
  [resource ^Cursor cursor]
  (raw-request
    (lsp.jsonrpc/notification
      "textDocument/hover"
      {:textDocument {:uri (resource-uri resource)}
       :position (editor-cursor->lsp-position cursor)})
    (bound-fn [result _]
      (when result
        (let [{:keys [contents range]} result
              range (or (some-> range lsp-range->editor-cursor-range)
                        (data/->CursorRange cursor (data/->Cursor (.-row cursor) (inc (.-col cursor)))))
              contents (if (vector? contents) contents [contents])]
          (->> contents
               (e/keep
                 (fn [content]
                   (let [region (hover-response->region range content)]
                     (when-not (-> region :content :value string/blank?)
                       region))))
               vec))))))

(defn prepare-rename
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_prepareRename"
  [resource cursor range-converter]
  (raw-request
    (lsp.jsonrpc/notification
      "textDocument/prepareRename"
      {:textDocument {:uri (resource-uri resource)}
       :position (editor-cursor->lsp-position cursor)})
    (bound-fn [result _]
      (when result
        (cond
          (:range result) (range-converter (lsp-range->editor-cursor-range (:range result)))
          (:start result) (range-converter (lsp-range->editor-cursor-range result)))))))

(defn rename
  "See also:
    https://microsoft.github.io/language-server-protocol/specifications/lsp/3.17/specification/#textDocument_rename"
  [resource cursor new-name]
  (raw-request
    (lsp.jsonrpc/notification
      "textDocument/rename"
      {:textDocument {:uri (resource-uri resource)}
       :position (editor-cursor->lsp-position cursor)
       :newName new-name})
    (bound-fn [result project]
      (lsp.async/with-auto-evaluation-context evaluation-context
        (->> result
             :changes
             (e/keep (fn [[k edits]]
                       (when-let [resource (maybe-resource project (str (symbol k)) evaluation-context)]
                         (coll/pair resource
                                    (->> edits
                                         (mapv text-edit:lsp->editor)
                                         (sort-by :cursor-range)
                                         (mapv (fn [{:keys [cursor-range value]}]
                                                 (coll/pair cursor-range (util/split-lines value)))))))))
             (into {}))))))

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

(ns integration.lua-indent-test
  "Checks our indentation against the Lua language server the editor ships with.

  Indent-on-type and format-on-save have to agree. When they do not, the user's
  file flips between the two shapes on every keystroke and every save, so this
  runs the real formatter rather than trusting a recording of what it once
  produced. The server version is pinned in project.clj under :packing."
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.data-test :refer [layout-info]]
            [editor.code.script :as script]
            [editor.defold-project :as project]
            [editor.lsp :as lsp]
            [editor.os :as os]
            [editor.system :as system]
            [integration.test-util :as test-util]
            [util.coll :as coll])
  (:import (com.dynamo.bob Platform)
           (java.io File)))

(set! *warn-on-reflection* true)

(def ^:private format-timeout-ms 30000)

(g/defnode LuaIndentViewNode
  (property diagnostics g/Any (default []))
  (property document-symbols g/Any (default []))
  (property completion-trigger-characters g/Any (default #{})))

(defn- language-server-root
  ^File []
  (io/file (system/defold-unpack-path)
           (.getPair (Platform/getHostPlatform))
           "bin/lsp/lua"))

(defn- lua-language-server
  "The same server the editor runs, see editor-extensions/built-in-lua-language-server."
  [^File root]
  {:languages #{"lua"}
   :launcher {:command [(str (io/file root "bin" (str "lua-language-server"
                                                      (when (os/is-win32?) ".exe"))))
                        (str "--configpath=" (io/file root "config.json"))]}})

(defn- await-format-edits
  "Formats resource, retrying until a server has started and answered.

  format-document! hands the callback nil when no running server advertises
  formatting, which is the case for as long as the process is still coming up."
  [lsp resource]
  (let [deadline (+ (System/currentTimeMillis) 60000)]
    (loop []
      (let [response (promise)]
        (lsp/format-document! lsp resource :four-spaces #(deliver response %)
                              :timeout-ms format-timeout-ms)
        (let [result (deref response format-timeout-ms nil)]
          (cond
            (map? result) (:edits result)

            (< (System/currentTimeMillis) ^long deadline)
            (do (Thread/sleep 250)
                (recur))

            :else
            (throw (ex-info "No Lua language server answered textDocument/formatting"
                            {:resource resource}))))))))

(deftest reindent-agrees-with-language-server-test
  (let [root (language-server-root)]
    (if-not (.exists (io/file root "bin"))
      ;; Only unpacked by `lein init`, which CI runs before `lein test`.
      (println "Skipping" `reindent-agrees-with-language-server-test
               "- no Lua language server at" (str root))
      (test-util/with-scratch-project "test/resources/lua_indent_project"
        (let [lsp (lsp/get-node-lsp project)
              view-node (first (g/tx-nodes-added
                                 (g/transact
                                   {:undoable false}
                                   (g/make-node (g/node-id->graph-id app-view) LuaIndentViewNode))))
              resource (test-util/resource workspace "/indent_test_cases.lua")
              lines (g/node-value (project/get-resource-node project resource) :lines)]
          (try
            (lsp/set-servers! lsp #{(lua-language-server root)})
            (lsp/open-view! lsp view-node resource lines)
            (let [edits (await-format-edits lsp resource)
                  formatted (if (coll/empty? edits)
                              lines
                              (:lines (data/apply-edits lines [] [] edits)))
                  reindented (or (:lines (data/reindent (data/indent-level-pattern 4) "    "
                                                        script/lua-grammar [] formatted
                                                        [(data/->CursorRange
                                                           (data/->Cursor 0 0)
                                                           (data/->Cursor (dec (count formatted))
                                                                          (count (peek formatted))))]
                                                        nil (layout-info formatted)))
                                 formatted)]
              ;; The fixture is checked in already formatted, so the server must
              ;; leave it alone. If this fails, the pinned server version changed
              ;; its style and the fixture needs regenerating.
              (is (= lines formatted)
                  "the language server no longer formats the fixture the way it is checked in")

              ;; And so must we. Cases the two genuinely disagree on are
              ;; commented out in the fixture, each with the reason and the
              ;; shape the server produced.
              (is (= (count formatted) (count reindented)))
              (dotimes [row (count formatted)]
                (is (= (formatted row) (reindented row)) (str "row " row))))
            (finally
              (lsp/close-view! lsp view-node)
              (lsp/set-servers! lsp #{})
              (lsp/await lsp))))))))

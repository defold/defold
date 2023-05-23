;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.code.resource
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.util :as util]
            [editor.lsp :as lsp]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [schema.core :as s]
            [util.text-util :as text-util])
  (:import [editor.code.data Cursor CursorRange]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def TCursor (s/record Cursor {:row Long :col Long}))
(def TCursorRange (s/record CursorRange {:from TCursor
                                         :to TCursor
                                         (s/optional-key :dragged) s/Bool}))
(def TRegion (s/record CursorRange {:from TCursor
                                    :to TCursor
                                    :type s/Keyword
                                    s/Keyword s/Any}))

(g/deftype BreakpointRows (s/constrained #{s/Num} sorted?))
(g/deftype Cursors [TCursor])
(g/deftype CursorRanges [TCursorRange])
(g/deftype IndentType (s/enum :tabs :two-spaces :four-spaces))
(g/deftype InvalidatedRows [Long])
(g/deftype Lines [String])
(g/deftype Regions [TRegion])
(g/deftype RegionGrouping {s/Any [TRegion]})

(def ^:private default-indent-type :tabs)

(defn make-code-error-user-data [^String path line-number]
  (let [cursor-range (some-> line-number data/line-number->CursorRange)]
    (cond-> {:filename path}

            (some? cursor-range)
            (assoc :cursor-range cursor-range))))

(defn guess-indent-type [lines]
  ;; TODO: Use default from preferences if indeterminate.
  (or (data/guess-indent-type (take 512 lines) 4)
      default-indent-type))

(defn read-fn [resource]
  (util/split-lines (slurp resource)))

(defn write-fn [lines]
  (string/join "\n" lines))

(defn search-value-fn [node-id resource evaluation-context]
  (let [loaded-lines (g/node-value node-id :save-value evaluation-context)]
    (if (g/error-value? loaded-lines)
      loaded-lines
      {:resource resource
       :loaded-lines loaded-lines})))

(defn search-fn
  ([search-string]
   (text-util/search-string->re-pattern search-string :case-insensitive))
  ([search-value re-pattern]
   (if-some [loaded-lines (:loaded-lines search-value)]
     (text-util/lines->text-matches loaded-lines re-pattern)
     (resource/resource->text-matches (:resource search-value) re-pattern))))

;; To save memory, we defer loading the file contents until it has been modified
;; and read directly from disk up to that point. Once the file is edited we need
;; the unmodified lines to be able to undo past the point when the file was last
;; saved. When the first edit is performed, we load the original lines from disk
;; and store it as user-data in the graph. We cannot store this in a property of
;; the node, because undoing would clear out the state of the unmodified file as
;; well, which would defeat the purpose.

(defn- read-unmodified-lines
  "Reads and returns the unmodified lines from the specified resource. If an
  error occurs, return an ErrorValue from the provided node-id and output."
  [resource error-node-id error-output-label]
  (resource-io/with-error-translation resource error-node-id error-output-label
    (read-fn resource)))

(defn- loaded-unmodified-lines
  "Returns the unmodified lines for the specified node-id, or nil if the node
  was never modified."
  [node-id]
  ;; Note: Be really careful how you use this. It will be initialized only once,
  ;; so any outputs that make use of it can potentially produce a stale value as
  ;; it bypasses the established cache invalidation mechanism.
  (g/user-data node-id :unmodified-lines))

(defn ensure-unmodified-lines!
  "Ensures the unmodified lines of a resource has been loaded, and returns the
  unmodified lines. The first time this is called for a node-id, the unmodified
  lines will be read from disk. Subsequent calls return the unmodified lines
  loaded during the first call. The loaded-unmodified-lines function will return
  a non-nil value after this has been called."
  [node-id evaluation-context]
  (let [did-change (volatile! false)
        unmodified-lines
        (g/user-data-swap! node-id :unmodified-lines
                       (fn [loaded-unmodified-lines]
                         (or loaded-unmodified-lines
                             (let [resource (g/node-value node-id :resource evaluation-context)
                                   lines (read-unmodified-lines resource node-id nil)]
                               (vreset! did-change true)
                               lines))))]
    (when @did-change
      (g/invalidate-outputs! [(g/endpoint node-id :save-value)]))
    unmodified-lines))

(defn- eager-load [self resource]
  (let [lines (read-fn resource)
        indent-type (guess-indent-type lines)]
    (g/user-data! self :unmodified-lines lines) ; Avoids disk reads property setters below.
    (g/set-property self
      :modified-lines lines
      :modified-indent-type indent-type)))

(defn- load-fn [additional-load-fn eager-loading? connect-breakpoints? project self resource]
  (concat
    (when eager-loading?
      (eager-load self resource))
    (when connect-breakpoints?
      (g/connect self :breakpoints project :breakpoints))
    (when additional-load-fn
      (additional-load-fn project self resource))))

(g/defnk produce-breakpoint-rows [regions]
  (into (sorted-set)
        (comp (filter data/breakpoint-region?)
              (map data/breakpoint-row))
        regions))

(g/defnode CodeEditorResourceNode
  (inherits resource-node/ResourceNode)

  (property cursor-ranges CursorRanges (default [data/document-start-cursor-range]) (dynamic visible (g/constantly false)))
  (property invalidated-rows InvalidatedRows (default []) (dynamic visible (g/constantly false)))
  (property modified-indent-type IndentType (dynamic visible (g/constantly false)))
  (property modified-lines Lines (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   ;; TODO(save-value): I reordered these calls because it seemed backwards. Should I have left them?
                   (ensure-unmodified-lines! self evaluation-context)
                   (lsp/notify-lines-modified! (lsp/get-node-lsp (:basis evaluation-context) self) self new-value evaluation-context)
                   nil)))

  (property regions Regions (default []) (dynamic visible (g/constantly false)))

  (output breakpoint-rows BreakpointRows :cached produce-breakpoint-rows)
  (output completions g/Any (g/constantly {}))
  (output indent-type IndentType :cached (g/fnk [_node-id modified-indent-type resource]
                                           (or modified-indent-type
                                               (let [lines (read-unmodified-lines resource _node-id :indent-type)]
                                                 (if (g/error? lines)
                                                   default-indent-type
                                                   (guess-indent-type lines))))))

  (output lines Lines (g/fnk [_node-id save-value resource] (or save-value (read-unmodified-lines resource _node-id :lines))))

  ;; To save memory, we don't store the source-value in the graph.
  ;; Instead, we use its hash to determine if we've been dirtied.
  ;; Thus, we have a specialized implementation of save-data.
  (output save-data g/Any :cached (g/fnk [_node-id resource save-value source-value]
                                    (let [dirty (and (some? save-value)
                                                     (not= source-value (hash save-value)))]
                                      (resource-node/make-save-data _node-id resource save-value dirty))))

  (output save-value Lines (g/fnk [_node-id modified-lines] (or modified-lines (loaded-unmodified-lines _node-id))))

  ;; Even though this returns a hash and not a proper source-value, we keep the
  ;; name to benefit from the invalidation mechanism when saving.
  (output source-value g/Any :cached :unjammable (g/fnk [_node-id resource]
                                                   (let [lines (read-unmodified-lines resource _node-id :source-value)]
                                                     (if (g/error? lines)
                                                       lines
                                                       (hash lines))))))

(defn register-code-resource-type [workspace & {:keys [ext node-type language icon view-types view-opts tags tag-opts label eager-loading? additional-load-fn] :as args}]
  (let [debuggable? (contains? tags :debuggable)
        load-fn (partial load-fn additional-load-fn eager-loading? debuggable?)
        args (-> args
                 (dissoc :additional-load-fn :eager-loading?)
                 (assoc :load-fn load-fn
                        :write-fn write-fn
                        :search-fn search-fn
                        :search-value-fn search-value-fn
                        :textual? true))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

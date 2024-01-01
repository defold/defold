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
            [schema.core :as s])
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
(g/deftype Region TRegion)
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

;; To save memory, we defer loading the file contents until it has been modified
;; and read directly from disk up to that point. Once the file is edited we need
;; the unmodified lines to be able to undo past the point when the file was last
;; saved. When the first edit is performed, we load the original lines from disk
;; and store it as user-data in the graph. We cannot store this in a property of
;; the node, because undoing would clear out the state of the unmodified file as
;; well, which would defeat the purpose.

(defn- read-lines+disk-sha256
  "Reads the disk state of the specified CodeEditorResourceNode id from disk and
  returns a pair of [lines, disk-sha256]. In case of errors, the lines key in
  the returned pair will be an ErrorValue and the disk-sha256 value will be nil.
  The disk-sha256 will also be nil for any resource that is not a file in the
  project."
  [node-id resource]
  (let [lines+disk-sha256 (resource-io/with-error-translation resource node-id nil
                            (resource/read-source-value+sha256-hex resource read-fn))]
    (if (g/error? lines+disk-sha256)
      [lines+disk-sha256 nil]
      lines+disk-sha256)))

(defn- loaded-unmodified-lines
  "Returns the unmodified lines for the specified node-id, or nil if the node
  was never modified."
  [node-id]
  (g/user-data node-id :unmodified-lines))

(defn- set-unmodified-lines!
  "Sets the unmodified lines for the specified node-id."
  [node-id lines]
  (g/user-data! node-id :unmodified-lines lines))

(defn- ensure-disk-state
  "Ensures the disk state has been loaded for the specified node-id. The first
  time this is called for a node-id, the state will be read from disk and the
  supplied seq of invalidated output labels will be invalidated. Subsequent
  calls will do nothing. The loaded-unmodified-lines function will return a
  non-nil value after this function is called."
  [node-id evaluation-context invalidated-labels]
  (let [read-volatile (volatile! nil)]
    (g/user-data-swap!
      node-id :unmodified-lines
      (fn [loaded-unmodified-lines]
        (or loaded-unmodified-lines
            (let [resource (g/node-value node-id :resource evaluation-context)
                  [lines-or-error-value disk-sha256] (read-lines+disk-sha256 node-id resource)]
              (vreset! read-volatile [resource disk-sha256])
              lines-or-error-value))))
    (when-some [[resource disk-sha256] (deref read-volatile)]
      (when (seq invalidated-labels)
        (g/invalidate-outputs! (mapv #(g/endpoint node-id %)
                                     invalidated-labels)))
      (when (some? disk-sha256)
        (let [workspace (resource/workspace resource)]
          (workspace/set-disk-sha256 workspace node-id disk-sha256))))))

(defn ensure-loaded!
  "Ensures a lazy-loaded CodeEditorResourceNode has loaded the contents of its
  associated resource into memory."
  [node-id evaluation-context]
  ;; This function is called externally for its side effects, so we need to
  ;; invalidate any outputs that depend on the disk-state.
  (some-> (ensure-disk-state node-id evaluation-context [:save-value])
          (g/transact)))

(defn- eager-load [self resource]
  (let [[lines disk-sha256] (resource/read-source-value+sha256-hex resource read-fn)
        indent-type (guess-indent-type lines)]
    (set-unmodified-lines! self lines) ; Avoids a disk read in modified-lines property setter.
    (cond-> (g/set-property self :modified-lines lines :modified-indent-type indent-type)
            disk-sha256 (concat (workspace/set-disk-sha256 (resource/workspace resource) self disk-sha256)))))

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
                   (lsp/notify-lines-modified! (lsp/get-node-lsp (:basis evaluation-context) self) self new-value evaluation-context)
                   (ensure-disk-state self evaluation-context [])))) ; No need to invalidate :save-value here since it depends on :modified-lines.

  (property regions Regions (default []) (dynamic visible (g/constantly false)))

  (output breakpoint-rows BreakpointRows :cached produce-breakpoint-rows)
  (output completions g/Any (g/constantly {}))
  (output indent-type IndentType :cached (g/fnk [_node-id modified-indent-type resource]
                                           (or modified-indent-type
                                               (let [lines (resource-io/with-error-translation resource _node-id :indent-type
                                                             (read-fn resource))]
                                                 (if (g/error? lines)
                                                   default-indent-type
                                                   (guess-indent-type lines))))))

  (output lines Lines (g/fnk [_node-id save-value resource] (or save-value
                                                                (resource-io/with-error-translation resource _node-id :lines
                                                                  (read-fn resource)))))

  (output save-value Lines (g/fnk [_node-id modified-lines] (or modified-lines (loaded-unmodified-lines _node-id))))

  ;; To save memory, save-data is not cached. It is trivial to produce.
  (output save-data g/Any resource-node/produce-save-data)

  ;; To save memory, we don't store the source-value in the graph.
  ;; Instead, we use its hash to determine if we've been dirtied.
  ;; This output must still be named source-value since it is
  ;; invalidated when saving.
  (output source-value g/Any :cached (g/fnk [_node-id editable resource]
                                       (when editable
                                         (let [lines (resource-io/with-error-translation resource _node-id :source-value
                                                       (read-fn resource))]
                                           (if (g/error? lines)
                                             lines
                                             (hash lines))))))

  ;; We're dirty if the hash of our non-nil save-value differs from the source.
  (output dirty? g/Bool (g/fnk [_node-id editable save-value source-value]
                          (and editable (some? save-value) (not= source-value (hash save-value))))))

(defn register-code-resource-type [workspace & {:keys [ext node-type language icon view-types view-opts tags tag-opts label eager-loading? additional-load-fn] :as args}]
  (let [debuggable? (contains? tags :debuggable)
        load-fn (partial load-fn additional-load-fn eager-loading? debuggable?)
        args (-> args
                 (dissoc :additional-load-fn :eager-loading?)
                 (assoc :load-fn load-fn
                        :read-fn read-fn
                        :write-fn write-fn
                        :textual? true
                        :test-info {:type :code}))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

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

(ns editor.code.resource
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.lsp :as lsp]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
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
(g/deftype Region TRegion)
(g/deftype Regions [TRegion])
(g/deftype RegionGrouping {s/Any [TRegion]})

(def default-indent-type :tabs)

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
  (data/string->lines (slurp resource)))

(defn write-fn [lines]
  (data/lines->string lines))

;; To save memory, we don't store the source-value in the graph.
;; Instead, we use its hash to determine if we've been dirtied.
(def source-value-fn hash)

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
  ;; Note: Be really careful how you use this. It will be initialized only once,
  ;; so any outputs that make use of it can potentially produce a stale value as
  ;; it bypasses the established cache invalidation mechanism.
  (g/user-data node-id :unmodified-lines))

(defn- set-unmodified-lines!
  "Sets the unmodified lines for the specified node-id."
  [node-id lines]
  (g/user-data! node-id :unmodified-lines lines))

(defn- init-disk-state
  "Ensures the disk state has been loaded for the specified node-id. The first
  time this is called for a node-id, the state will be read from disk and
  returned. Subsequent calls will simply return nil. The loaded-unmodified-lines
  function will return a non-nil value after this function is called."
  [node-id evaluation-context]
  (let [read-volatile (volatile! nil)]
    (g/user-data-swap!
      node-id :unmodified-lines
      (fn [loaded-unmodified-lines]
        (or loaded-unmodified-lines
            (let [resource (g/node-value node-id :resource evaluation-context)
                  [lines-or-error-value disk-sha256] (read-lines+disk-sha256 node-id resource)]
              (vreset! read-volatile [resource lines-or-error-value disk-sha256])
              lines-or-error-value))))
    (when-some [[resource lines-or-error-value disk-sha256] (deref read-volatile)]
      (let [resource-type (resource/resource-type resource)
            source-value (resource-node/save-value->source-value lines-or-error-value resource-type)]
        [resource source-value disk-sha256]))))

(defn ensure-loaded!
  "Ensures a lazy-loaded CodeEditorResourceNode has loaded the contents of its
  associated resource into memory."
  [node-id evaluation-context]
  ;; This function is called externally for its side effects, so we need to
  ;; invalidate any outputs that depend on the disk-state.
  (when-some [[resource source-value disk-sha256] (init-disk-state node-id evaluation-context)]
    (resource-node/set-source-value! node-id source-value)
    (when disk-sha256
      (g/transact
        (workspace/set-disk-sha256 (resource/workspace resource) node-id disk-sha256)))))

(defn- eager-load [self lines]
  {:pre [(and (vector? lines)
              (string? (first lines)))]}
  (let [indent-type (guess-indent-type lines)]
    (set-unmodified-lines! self lines) ; Avoids a disk read in modified-lines property setter.
    (g/set-properties self
      :modified-lines lines
      :modified-indent-type indent-type)))

(defn- load-fn [additional-load-fn lazy-loaded connect-breakpoints project self resource lines]
  (concat
    (when-not lazy-loaded
      (eager-load self lines))
    (when connect-breakpoints
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

  (property modified-lines types/Lines (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         lsp (lsp/get-node-lsp basis self)]
                     (if-some [[resource source-value disk-sha256] (init-disk-state self evaluation-context)]
                       (do
                         (lsp/notify-lines-modified! lsp resource source-value new-value)
                         (resource-node/set-source-value! self source-value)
                         (when disk-sha256
                           (workspace/set-disk-sha256 (resource/workspace resource) self disk-sha256)))
                       (let [resource (resource-node/resource basis self)
                             source-value (g/node-value self :source-value evaluation-context)]
                         (lsp/notify-lines-modified! lsp resource source-value new-value)
                         nil))))))

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

  (output lines types/Lines (g/fnk [_node-id save-value resource] (or save-value
                                                                      (resource-io/with-error-translation resource _node-id :lines
                                                                        (read-fn resource)))))

  (output save-value types/Lines (g/fnk [_node-id modified-lines]
                                   (or modified-lines
                                       (loaded-unmodified-lines _node-id)))))

(defn code-resource-type? [resource-type]
  (and (:textual? resource-type)
       (identical? read-fn (:read-fn resource-type))
       (identical? write-fn (:write-fn resource-type))))

(defn register-code-resource-type [workspace & {:keys [ext node-type language icon view-types view-opts tags tag-opts label lazy-loaded additional-load-fn built-pb-class] :as args}]
  (let [connect-breakpoints (contains? tags :debuggable)
        load-fn (partial load-fn additional-load-fn lazy-loaded connect-breakpoints)
        args (-> args
                 (dissoc :additional-load-fn)
                 (assoc :load-fn load-fn
                        :read-fn read-fn
                        :write-fn write-fn
                        :search-fn search-fn
                        :search-value-fn search-value-fn
                        :source-value-fn source-value-fn
                        :textual? true
                        :test-info (cond-> {:type :code}
                                           built-pb-class (assoc :built-pb-class built-pb-class))))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

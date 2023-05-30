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
            [util.digest :as digest])
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

;; To save memory, we defer loading the file contents until it has been modified
;; and read directly from disk up to that point. Once the file is edited we need
;; the unmodified lines to be able to undo past the point when the file was last
;; saved. When the first edit is performed, we load the original state from disk
;; and store it as user-data in the graph. We cannot store this in a property of
;; the node, because undoing would clear out the state of the unmodified file as
;; well, which would defeat the purpose.

(defn- make-disk-state
  "Makes a new map representing the unaltered on-disk state for a file. It
  includes the unmodified lines and the SHA-256 of the raw disk bytes."
  [disk-lines disk-sha256]
  {:pre [(or (vector? disk-lines)
           (g/error-value? disk-lines))
         (or (digest/sha256-hex? disk-sha256)
           (nil? disk-sha256))]}
  {:disk-lines disk-lines
   :disk-sha256 disk-sha256})

(defn- read-disk-state
  "Reads the disk state of the specified CodeEditorResourceNode id from disk.
  In case of errors, the :disk-lines key in the returned map will be an
  ErrorValue and the :disk-sha256 will be nil. The :disk-sha256 will also be nil
  for any resource that is not a file in the project."
  [node-id evaluation-context]
  (let [resource (g/node-value node-id :resource evaluation-context)
        result (resource-io/with-error-translation resource node-id nil
                 (resource/read-source-value+sha256-hex resource read-fn))]
    (if (g/error? result)
      (make-disk-state result nil)
      (let [[disk-lines disk-sha256] result]
        (make-disk-state disk-lines disk-sha256)))))

(defn- loaded-disk-state
  "Returns a map representing the on-disk state for the specified node-id, or
  nil if the node makes use of lazy loading and have not been loaded yet."
  [node-id]
  (g/user-data node-id :disk-state))

(defn- ensure-disk-state!
  "Ensures the disk state has been loaded for the specified node-id. The first
  time this is called for a node-id, the state will be read from disk and the
  supplied seq of invalidated output labels will be invalidated. Subsequent
  calls will do nothing. The loaded-disk-state function will return a non-nil
  value after this function is called."
  [node-id evaluation-context invalidated-labels]
  (let [did-load-volatile (volatile! false)]
    (g/user-data-swap!
      node-id :disk-state
      (fn [loaded-disk-state]
        (or loaded-disk-state
            (let [disk-state (read-disk-state node-id evaluation-context)]
              (vreset! did-load-volatile true)
              disk-state))))
    (when (and (deref did-load-volatile)
               (seq invalidated-labels))
      (g/invalidate-outputs! (mapv #(g/endpoint node-id %)
                                   invalidated-labels)))))

(defn ensure-loaded!
  "Ensures a lazy-loaded CodeEditorResourceNode has loaded the contents of its
  associated resource into memory."
  [node-id evaluation-context]
  ;; This function is called externally for its side effects, so we need to
  ;; invalidate any outputs that depend on the disk-state.
  (ensure-disk-state! node-id evaluation-context [:disk-sha256 :save-value]))

(defn- eager-load [self resource]
  (let [[lines disk-sha256] (resource/read-source-value+sha256-hex resource read-fn)
        indent-type (guess-indent-type lines)
        disk-state (make-disk-state lines disk-sha256)]
    ;; This avoids a disk read in modified-lines property setter. There's no
    ;; need to invalidate any outputs dependent on the disk-state here, since
    ;; we're still in the loading phase.
    (g/user-data! self :disk-state disk-state)
    (g/set-property self :modified-lines lines :modified-indent-type indent-type)))

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
                   (ensure-disk-state! self evaluation-context [:disk-sha256])))) ; No need to invalidate :save-value here since it depends on :modified-lines.

  (property regions Regions (default []) (dynamic visible (g/constantly false)))

  ;; Covers property disk-sha256 in ResourceNode.
  ;; Reads from :disk-state node user-data in order to support lazy-loading.
  ;; Will produce nil if the contents has not yet been loaded, in which case the
  ;; node cannot participate in the keep-if-contents-unchanged check during
  ;; resource sync. The disk-state will be loaded the first time the
  ;; modified-lines property is set.
  (output disk-sha256 g/Str :unjammable (g/fnk [_node-id] (:disk-sha256 (loaded-disk-state _node-id))))

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

  (output save-value Lines (g/fnk [_node-id modified-lines] (or modified-lines (:disk-lines (loaded-disk-state _node-id)))))

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
  (output dirty? g/Bool (g/fnk [_node-id editable resource save-value source-value]
                          (and editable (some? save-value) (not= source-value (hash save-value))))))

(defn register-code-resource-type [workspace & {:keys [ext node-type language icon view-types view-opts tags tag-opts label eager-loading? additional-load-fn] :as args}]
  (let [debuggable? (contains? tags :debuggable)
        load-fn (partial load-fn additional-load-fn eager-loading? debuggable?)
        args (-> args
                 (dissoc :additional-load-fn :eager-loading?)
                 (assoc :load-fn load-fn
                        :read-fn read-fn
                        :write-fn write-fn
                        :textual? true))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

(ns editor.code.resource
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.util :as util]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [schema.core :as s])
  (:import (editor.code.data Cursor CursorRange)))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def TCursor (s/record Cursor {:row Long :col Long}))
(def TCursorRange (s/record CursorRange {:from TCursor :to TCursor}))
(def TRegion (s/record CursorRange {:from TCursor
                                    :to TCursor
                                    :type s/Keyword
                                    s/Keyword s/Any}))

(g/deftype BreakpointRows (sorted-set s/Num))
(g/deftype Cursors [TCursor])
(g/deftype CursorRanges [TCursorRange])
(g/deftype IndentType (s/enum :tabs :two-spaces :four-spaces))
(g/deftype InvalidatedRows [Long])
(g/deftype Lines [String])
(g/deftype Regions [TRegion])
(g/deftype RegionGrouping {s/Any [TRegion]})

(defn guess-indent-type [lines]
  ;; TODO: Use default from preferences if indeterminate.
  (or (data/guess-indent-type (take 512 lines) 4)
      :tabs))

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

(defn- loaded-unmodified-lines
  "Returns the unmodified lines for the specified node-id, or nil if the node
  was never modified."
  [node-id]
  (g/user-data node-id :unmodified-lines))

(defn- set-unmodified-lines!
  "Sets the unmodified lines for the specified node-id."
  [node-id lines]
  (g/user-data! node-id :unmodified-lines lines))

(defn- ensure-unmodified-lines!
  "Ensures the unmodified lines of a resource has been loaded, and returns the
  unmodified lines. The first time this is called for a node-id, the unmodified
  lines will be read from disk. Subsequent calls return the unmodified lines
  loaded during the first call. The loaded-unmodified-lines function will return
  a non-nil value after this has been called."
  [node-id evaluation-context]
  (g/user-data-swap! node-id :unmodified-lines
                     (fn [loaded-unmodified-lines]
                       (or loaded-unmodified-lines
                           (read-fn (g/node-value node-id :resource evaluation-context))))))

(defn- eager-load [self resource]
  (let [lines (read-fn resource)
        indent-type (guess-indent-type lines)]
    (set-unmodified-lines! self lines) ; Avoids a disk read in modified-lines property setter.
    (g/set-property self :modified-lines lines :modified-indent-type indent-type)))

(defn- load-fn [eager-loading? connect-breakpoints? project self resource]
  (concat
    (when eager-loading?
      (eager-load self resource))
    (when connect-breakpoints?
      (g/connect self :breakpoints project :breakpoints))))

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
            (set (fn [evaluation-context self _old-value _new-value]
                   (ensure-unmodified-lines! self evaluation-context)
                   nil)))
  (property regions Regions (default []) (dynamic visible (g/constantly false)))

  (output breakpoint-rows BreakpointRows :cached produce-breakpoint-rows)
  (output completions g/Any (g/constantly {}))
  (output indent-type IndentType :cached (g/fnk [modified-indent-type resource] (or modified-indent-type (guess-indent-type (read-fn resource)))))
  (output lines Lines (g/fnk [save-value resource] (or save-value (read-fn resource))))
  (output save-value Lines (g/fnk [_node-id modified-lines] (or modified-lines (loaded-unmodified-lines _node-id))))

  ;; To save memory, save-data is not cached. It is trivial to produce.
  (output save-data g/Any resource-node/produce-save-data)

  ;; To save memory, we don't store the source-value in the graph.
  ;; Instead we use its hash to determine if we've been dirtied.
  ;; This output must still be named source-value since it is
  ;; invalidated when saving.
  (output source-value g/Any :cached (g/fnk [editable? resource]
                                       (when editable?
                                         (hash (read-fn resource)))))

  ;; We're dirty if the hash of our non-nil save-value differs from the source.
  (output dirty? g/Bool (g/fnk [_node-id editable? resource save-value source-value]
                          (and editable? (some? save-value) (not= source-value (hash save-value))))))

(defn register-code-resource-type [workspace & {:keys [ext node-type icon view-types view-opts tags tag-opts label eager-loading?] :as args}]
  (let [debuggable? (contains? tags :debuggable)
        load-fn (partial load-fn eager-loading? debuggable?)
        args (-> args
                 (dissoc :eager-loading?)
                 (assoc :load-fn load-fn
                        :read-fn read-fn
                        :write-fn write-fn
                        :textual? true))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

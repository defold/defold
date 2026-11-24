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
            [editor.graph-util :as gu]
            [editor.lsp :as lsp]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [schema.core :as s]
            [util.eduction :as e]
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

(defn read-fn [_read-opts _owner-resource readable]
  (data/string->lines (slurp readable)))

(defn write-fn [lines]
  (data/lines->string lines))

(defn search-fn
  ([search-string]
   (text-util/search-string->re-pattern search-string :case-insensitive))
  ([lines re-pattern]
   (text-util/lines->text-matches lines re-pattern)))

(defn- connect-fn [additional-connect-fn connect-breakpoints project self resource]
  (e/concat
    (when connect-breakpoints
      (g/connect self :breakpoints project :breakpoints))
    (when additional-connect-fn
      (additional-connect-fn project self resource))))

(defn- load-fn [additional-load-fn load-opts {self :node-id lines :source-value :as node-load-info}]
  (e/concat
    ;; Search may already have cached these lines on the shell. A clean editable
    ;; file shares one vector between :lines and :source-value. Undoing the
    ;; first edit restores those lines even after a save.
    (when lines
      (g/set-properties self
        :lines lines
        :indent-type (guess-indent-type lines)))
    (when additional-load-fn
      (additional-load-fn load-opts node-load-info))))

(g/defnk produce-breakpoint-rows [regions]
  (into (sorted-set)
        (comp (filter data/breakpoint-region?)
              (map data/breakpoint-row))
        regions))

(g/defnode CodeEditorResourceNode
  (inherits resource-node/ResourceNode)

  (property cursor-ranges CursorRanges (default [data/document-start-cursor-range]) (dynamic visible (g/constantly false)))
  (property invalidated-rows InvalidatedRows (default []) (dynamic visible (g/constantly false)))
  (property indent-type IndentType (default default-indent-type) (dynamic visible (g/constantly false)))

  (property lines types/Lines (dynamic visible (g/constantly false))
            (set (fn [{:keys [basis-ref] :as evaluation-context} self _old-value new-value]
                   (let [resource (resource-node/resource @basis-ref self)

                         source-value
                         (g/node-value self :source-value evaluation-context)]
                     (lsp/notify-lines-modified! (lsp/get-lsp @basis-ref) resource source-value new-value)
                     nil))))

  (property regions Regions (default []) (dynamic visible (g/constantly false)))

  (output breakpoint-rows BreakpointRows :cached produce-breakpoint-rows)
  (output completions g/Any (g/constantly {}))
  (output save-value types/Lines (gu/passthrough lines)))

(defn code-resource-type? [resource-type]
  (and (:textual? resource-type)
       (identical? read-fn (:read-fn resource-type))
       (identical? write-fn (:write-fn resource-type))))

(defn register-code-resource-type [workspace & {:keys [additional-load-fn built-pb-class tags] :as args}]
  (let [connect-breakpoints (contains? tags :debuggable)
        additional-connect-fn (:connect-fn args)
        resource-load-fn (partial load-fn additional-load-fn)

        resource-connect-fn
        (when (or additional-connect-fn connect-breakpoints)
          (partial connect-fn additional-connect-fn connect-breakpoints))

        args
        (cond-> (-> args
                    (dissoc :additional-load-fn :connect-fn)
                    (assoc :load-fn resource-load-fn
                           :read-fn read-fn
                           :write-fn write-fn
                           :search-fn search-fn
                           :textual? true
                           :test-info (cond-> {:type :code}
                                        built-pb-class (assoc :built-pb-class built-pb-class))))
          resource-connect-fn (assoc :connect-fn resource-connect-fn))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

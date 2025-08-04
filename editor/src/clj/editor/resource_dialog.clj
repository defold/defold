;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.resource-dialog
  (:require [cljfx.fx.h-box :as fx.h-box]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [util.fn :as fn]
            [util.thread-util :as thread-util]))

(def ^:private fuzzy-resource-filter-fn (partial fuzzy-choices/filter-options resource/proj-path resource/proj-path))

(defn- resource-node-ids->project-resources [basis resource-node-ids]
  (let [project-resources
        (into []
              (comp
                (map thread-util/abortable-identity!)
                (map #(resource-node/resource basis %))
                (filter (fn [resource]
                          (and (some? (resource/proj-path resource))
                               (resource/exists? resource)))))
              resource-node-ids)]
    (thread-util/throw-if-interrupted!)
    (sort-by resource/proj-path project-resources)))

(defn- node-ids->owner-resource-node-ids [basis node-ids]
  (into #{}
        (keep #(resource-node/owner-resource-node-id basis %))
        node-ids))

(defn- deps-filter-fn [project filter-value _items]
  ;; Dependencies are files that this file depends on. I.e. the atlas this sprite uses.
  (g/with-auto-evaluation-context evaluation-context
    (when-some [resource-node-id (project/get-resource-node project filter-value evaluation-context)]
      (let [basis (:basis evaluation-context)
            nodes-in-resource (core/recursive-owned-node-ids basis resource-node-id)

            nodes-we-depend-on
            (into #{}
                  (comp
                    (map thread-util/abortable-identity!)
                    (mapcat #(g/explicit-arcs-by-target basis %))
                    (map gt/source-id))
                  nodes-in-resource)

            resource-nodes-we-depend-on
            (disj (node-ids->owner-resource-node-ids basis nodes-we-depend-on)
                  resource-node-id)]

        (resource-node-ids->project-resources basis resource-nodes-we-depend-on)))))

(defn- refs-filter-fn [project filter-value _items]
  ;; Referencing files are files that depend on this file. I.e. the four sprites that refer to this atlas.
  (g/with-auto-evaluation-context evaluation-context
    (when-some [resource-node-id (project/get-resource-node project filter-value evaluation-context)]
      (let [basis (:basis evaluation-context)
            nodes-in-resource (core/recursive-owned-node-ids basis resource-node-id)
            recursive-overrides-fn #(g/overrides basis %)

            all-overrides-of-nodes-in-resource
            (eduction
              (mapcat #(tree-seq some? recursive-overrides-fn %))
              nodes-in-resource)

            nodes-that-depend-on-us
            (into #{}
                  (comp
                    (map thread-util/abortable-identity!)
                    (mapcat #(g/explicit-arcs-by-source basis %))
                    (map gt/target-id))
                  all-overrides-of-nodes-in-resource)

            resource-nodes-that-depend-on-us
            (disj (node-ids->owner-resource-node-ids basis nodes-that-depend-on-us)
                  resource-node-id)]

        (resource-node-ids->project-resources basis resource-nodes-that-depend-on-us)))))

(defn matched-list-item-view [{:keys [icon text matching-indices children]}]
  {:fx/type fx.h-box/lifecycle
   :style-class "content"
   :alignment :center-left
   :spacing 4
   :children (cond-> [{:fx/type ui/image-icon
                       :path icon
                       :size 16.0}
                      (fuzzy-choices/make-matched-text-flow-cljfx text matching-indices)]
                     children
                     (into children))})

(defn make [workspace project options]
  (let [exts         (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) fn/constantly-true)
        accept-fn    (or (:accept-fn options) fn/constantly-true)
        items        (into []
                           (filter #(and (= :file (resource/source-type %))
                                         (accepted-ext (resource/type-ext %))
                                         (resource/loaded? %)
                                         (not (resource/internal? %))
                                         (accept-fn %)))
                           (g/node-value workspace :resource-list))
        tooltip-gen (:tooltip-gen options)
        special-filter-fns {"refs" (partial refs-filter-fn project)
                            "deps" (partial deps-filter-fn project)}
        options (-> {:title "Select Resource"
                     :cell-fn (fn cell-fn [r]
                                (let [text (resource/proj-path r)
                                      icon (workspace/resource-icon r)
                                      tooltip (when tooltip-gen (tooltip-gen r))
                                      matching-indices (:matching-indices (meta r))]
                                  (cond-> {:style-class (into ["list-cell"] (resource/style-classes r))
                                           :graphic {:fx/type matched-list-item-view
                                                     :icon icon
                                                     :text text
                                                     :matching-indices matching-indices}}
                                          tooltip
                                          (assoc :tooltip tooltip))))
                     :filter-fn (fn filter-fn [filter-value items]
                                  (let [[command arg] (let [parts (string/split filter-value #":")]
                                                        (if (< 1 (count parts))
                                                          parts
                                                          [nil (first parts)]))
                                        f (get special-filter-fns command fuzzy-resource-filter-fn)]
                                    (f arg items)))}
                    (merge options))]
    (dialogs/make-select-list-dialog items options)))
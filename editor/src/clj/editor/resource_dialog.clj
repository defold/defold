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

(ns editor.resource-dialog
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.icons :as icons]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.ui.fuzzy-choices :as fuzzy-choices])
  (:import  [javafx.scene.layout HBox VBox]
            [javafx.scene.text Text TextFlow]
            [javafx.geometry Pos]))

(def ^:private fuzzy-resource-filter-fn (partial fuzzy-choices/filter-options resource/proj-path resource/proj-path))

(defn- file-scope [node-id]
  (last (take-while (fn [n] (and n (not (g/node-instance? project/Project n)))) (iterate core/scope node-id))))


(defn- sub-nodes [n]
  (g/node-value n :nodes))

(defn- sub-seq [n]
  (tree-seq (partial g/node-instance? resource-node/ResourceNode) sub-nodes n))

(defn- override-seq [node-id]
  (tree-seq g/overrides g/overrides node-id))

(defn- refs-filter-fn [project filter-value items]
  ;; Temp limitation to avoid stalls
  ;; Optimally we would do the work in the background with a progress-bar
  (if-let [n (project/get-resource-node project filter-value)]
    (->>
      (let [all (override-seq n)]
        (mapcat (fn [n]
                  (keep (fn [[src src-label node-id label]]
                          (when-let [node-id (file-scope node-id)]
                            (when (and (not= n node-id)
                                       (g/node-instance? resource-node/ResourceNode node-id))
                              (when-let [r (g/node-value node-id :resource)]
                                (when (resource/exists? r)
                                  r)))))
                        (g/outputs n)))
                all))
      distinct)
    []))


(defn- deps-filter-fn [project filter-value items]
  ;; Temp limitation to avoid stalls
  ;; Optimally we would do the work in the background with a progress-bar
  (if-let [node-id (project/get-resource-node project filter-value)]
    (->>
      (let [all (sub-seq node-id)]
        (mapcat
          (fn [n]
            (keep (fn [[src src-label tgt tgt-label]]
                    (when-let [src (file-scope src)]
                      (when (and (not= node-id src)
                                 (g/node-instance? resource-node/ResourceNode src))
                        (when-let [r (g/node-value src :resource)]
                          (when (resource/exists? r)
                            r)))))
                  (g/inputs n)))
          all))
      distinct)
    []))

(defn- make-text-run [text style-class]
  (let [text-view (Text. text)]
    (when (some? style-class)
      (.add (.getStyleClass text-view) style-class))
    text-view))

(defn- matched-text-runs [text matching-indices]
  (let [/ (or (some-> text (string/last-index-of \/) inc) 0)]
    (into []
          (mapcat (fn [[matched? start end]]
                    (cond
                      matched?
                      [(make-text-run (subs text start end) "matched")]

                      (< start / end)
                      [(make-text-run (subs text start /) "diminished")
                       (make-text-run (subs text / end) nil)]

                      (<= start end /)
                      [(make-text-run (subs text start end) "diminished")]

                      :else
                      [(make-text-run (subs text start end) nil)])))
          (fuzzy-text/runs (count text) matching-indices))))

(defn- make-matched-list-item-graphic [icon text matching-indices]
  (let [icon-view (icons/get-image-view icon 16)
        text-view (TextFlow. (into-array Text (matched-text-runs text matching-indices)))]
    (doto (HBox. (ui/node-array [icon-view text-view]))
      (.setAlignment Pos/CENTER_LEFT)
      (.setSpacing 4.0))))


(defn make-resource-dialog [workspace project options]
  (let [exts         (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) (constantly true))
        accept-fn    (or (:accept-fn options) (constantly true))
        items        (into []
                           (filter #(and (= :file (resource/source-type %))
                                         (accepted-ext (resource/type-ext %))
                                         (not (resource/internal? %))
                                         (accept-fn %)))
                           (g/node-value workspace :resource-list))
        options (-> {:title "Select Resource"
                     :prompt "Type to filter"
                     :cell-fn (fn [r]
                                (let [text (resource/proj-path r)
                                      icon (resource/resource-icon r)
                                      style (resource/style-classes r)
                                      tooltip (when-let [tooltip-gen (:tooltip-gen options)] (tooltip-gen r))
                                      matching-indices (:matching-indices (meta r))]
                                  (cond-> {:style style
                                           :tooltip tooltip}

                                          (empty? matching-indices)
                                          (assoc :icon icon :text text)

                                          :else
                                          (assoc :graphic (make-matched-list-item-graphic icon text matching-indices)))))
                     :filter-fn (fn [filter-value items]
                                  (let [fns {"refs" (partial refs-filter-fn project)
                                             "deps" (partial deps-filter-fn project)}
                                        [command arg] (let [parts (string/split filter-value #":")]
                                                        (if (< 1 (count parts))
                                                          parts
                                                          [nil (first parts)]))
                                        f (get fns command fuzzy-resource-filter-fn)]
                                    (f arg items)))}
                  (merge options))]
    (ui/make-select-list-dialog items options)))



(ns editor.resource-dialog
  (:require [editor.resource :as resource]
            [dynamo.graph :as g]
            [editor.workspace :as workspace]
            [cljfx.fx.h-box :as fx.h-box]
            [editor.ui :as ui]
            [cljfx.fx.text-flow :as fx.text-flow]
            [clojure.string :as string]
            [cljfx.fx.text :as fx.text]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.defold-project :as project]
            [editor.core :as core]
            [editor.resource-node :as resource-node]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.dialogs :as dialogs]))

(def ^:private fuzzy-resource-filter-fn (partial fuzzy-choices/filter-options resource/proj-path resource/proj-path))

(defn- sub-nodes [n]
  (g/node-value n :nodes))

(defn- sub-seq [n]
  (tree-seq (partial g/node-instance? resource-node/ResourceNode) sub-nodes n))

(defn- file-scope [node-id]
  (last (take-while (fn [n] (and n (not (g/node-instance? project/Project n)))) (iterate core/scope node-id))))

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

(defn- make-text-run [text style-class]
  (cond-> {:fx/type fx.text/lifecycle
           :text text}
          style-class
          (assoc :style-class style-class)))

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

(defn- matched-list-item-view [{:keys [icon text matching-indices]}]
  {:fx/type fx.h-box/lifecycle
   :style-class "content"
   :alignment :center-left
   :spacing 4
   :children [{:fx/type ui/image-icon
               :path icon
               :size 16.0}
              {:fx/type fx.text-flow/lifecycle
               :children (matched-text-runs text matching-indices)}]})

(defn make [workspace project options]
  (let [exts         (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) (constantly true))
        accept-fn    (or (:accept-fn options) (constantly true))
        items        (into []
                           (filter #(and (= :file (resource/source-type %))
                                         (accepted-ext (resource/type-ext %))
                                         (not (resource/internal? %))
                                         (accept-fn %)))
                           (g/node-value workspace :resource-list))
        tooltip-gen (:tooltip-gen options)
        options (-> {:title "Select Resource"
                     :cell-fn (fn [r]
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
    (dialogs/make-select-list-dialog items options)))
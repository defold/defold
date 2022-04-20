(ns editor.reveal
  (:require [vlaaad.reveal :as r]
            [dynamo.graph :as g]
            [internal.system :as is]
            [clojure.main :as m]))

(defn- node-value-or-err [ec node-id label]
  (try
    [(g/node-value node-id label ec) nil]
    (catch Exception e
      [nil e])))

(defn- node-value-or-err->sf [[v e]]
  (if e
    (let [{:clojure.error/keys [cause class]}
          (-> e Throwable->map m/ex-triage)]
      (r/as e (r/raw-string (or cause class) {:fill :error})))
    (r/stream v)))

(defn- node-id-sf [basis node-id]
  (let [type-sym (symbol (:k (g/node-type* basis node-id)))]
    (r/raw-string (str type-sym "#" node-id) {:fill :scalar})))

(declare label-tree-node)

(defn- node-children-fn [system basis ec node-id]
  (fn []
    (let [node-type-def @(g/node-type* basis node-id)]
      (->> [:input :property :output]
           (mapcat (fn [k] (keys (get node-type-def k))))
           distinct
           sort
           (map
             (fn [label]
               (label-tree-node system basis ec node-id label)))))))

(defn- label-tree-node [system basis ec node-id label]
  (let [[v e :as v-or-e] (node-value-or-err ec node-id label)
        sources (g/sources-of basis node-id label)
        targets (g/targets-of basis node-id label)]
    (cond->
      {:value (or e v)
       :render (r/horizontal
                 (r/stream label)
                 r/separator
                 (node-value-or-err->sf v-or-e))}
      (or (seq sources) (seq targets))
      (assoc :children #(map (fn [[relation [rel-node-id related-label]]]
                               (let [[v e :as v-or-e] (node-value-or-err ec node-id label)]
                                 {:value (or e v)
                                  :render (r/horizontal
                                            (r/raw-string
                                              ({:source "<- " :target "-> "} relation)
                                              {:fill :util})
                                            (r/stream related-label)
                                            (r/raw-string " of " {:fill :util})
                                            (node-id-sf basis rel-node-id)
                                            (r/raw-string ": " {:fill :util})
                                            (node-value-or-err->sf v-or-e))
                                  :children (node-children-fn system basis ec rel-node-id)}))
                             (concat (map (fn [x] [:source x]) sources)
                                     (map (fn [x] [:target x]) targets)))))))

(defn root-tree-node [system basis ec node-id]
  (let [node-type-def @(g/node-type* basis node-id)]
    {:value node-id
     :render (r/horizontal (r/raw-string (str
                                           (:name node-type-def)
                                           "#"
                                           node-id)
                                         {:fill :scalar}))
     :children (node-children-fn system basis ec node-id)}))

(r/defaction ::defold:node-tree [x]
  (when (g/node-id? x)
    (let [sys @g/*the-system*
          basis (is/basis sys)]
      (when (g/node-by-id basis x)
        (fn []
          (let [ec (is/default-evaluation-context sys)]
            {:fx/type r/tree-view
             :branch? :children
             :render #(:render % (:value %))
             :valuate :value
             :children #((:children %))
             :root (root-tree-node sys basis ec x)}))))))


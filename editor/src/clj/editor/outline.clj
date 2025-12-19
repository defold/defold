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

(ns editor.outline
  (:require [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.id :as id]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [internal.cache :as c]
            [internal.graph.types :as gt]
            [schema.core :as s]
            [service.log :as log]
            [util.coll :as coll])
  (:import [com.defold.editor.localization MessagePattern]
           [internal.graph.types Arc]))

(set! *warn-on-reflection* true)

(defprotocol ItemIterator
  (value [this])
  (parent [this]))

(defn- req-satisfied? [req node]
  (g/node-instance*? (:node-type req) node))

(defn- find-req [node reqs]
  (when reqs
    (first (filter #(req-satisfied? % node) reqs))))

(defn- serialize
  [fragment]
  (g/write-graph fragment (core/write-handlers)))

(defn- match-reqs [root-nodes target-item]
  (when-let [all-reqs (:child-reqs target-item)]
    (loop [root-nodes root-nodes
           matched-reqs []]
      (if-let [node (first root-nodes)]
        (if-let [match (find-req node all-reqs)]
          (recur (rest root-nodes) (conj matched-reqs match))
          nil)
        (when (seq matched-reqs)
          [target-item matched-reqs])))))

(defn- find-target-item [item-iterator root-nodes]
  (if item-iterator
    (->> item-iterator
      (iterate parent)
      (take-while some?)
      (mapcat #(let [item (value %)] [item (:alt-outline item)]))
      (some (partial match-reqs root-nodes)))
    nil))

(defn- valid-link? [value]
  (resource/resource? value))

(g/deftype OutlineData {:node-id                              s/Int
                        :node-outline-key                     (s/maybe s/Str)
                        :label                                (s/conditional
                                                                string? s/Str
                                                                localization/message-pattern? MessagePattern)
                        :icon                                 s/Str
                        (s/optional-key :link)                (s/maybe (s/pred valid-link?))
                        (s/optional-key :children)            [s/Any]
                        (s/optional-key :child-reqs)          [s/Any]
                        (s/optional-key :outline-error?)      s/Bool
                        (s/optional-key :outline-overridden?) s/Bool
                        (s/optional-key :outline-reference?)  s/Bool
                        (s/optional-key :outline-show-link?)  s/Bool
                        s/Keyword                             s/Any})

(g/defnode OutlineNode
  (input source-outline OutlineData)
  (input child-outlines OutlineData :array)
  (output node-outline OutlineData :abstract))

(defn- outline-attachments
  [node-id]
  (let [{:keys [children]} (g/node-value node-id :node-outline)
        child-ids (map :node-id children)]
    (into (mapv (fn [child-id]
                  [node-id child-id]) child-ids)
          (mapcat outline-attachments child-ids))))

(defn- add-attachments
  [fragment root-ids]
  (let [{:keys [node-id->serial-id]} fragment
        original-attachments (into []
                                   (mapcat outline-attachments)
                                   root-ids)
        tx-attach-arcs (into #{}
                             (mapcat (fn [[parent-id child-id]]
                                       (let [parent-outline (g/node-value parent-id :node-outline)
                                             child-node (g/node-by-id child-id)
                                             [item [req]] (or (match-reqs [child-node] parent-outline)
                                                              (match-reqs [child-node] (:alt-outline parent-outline)))]
                                         (when-some [tx-attach-fn (:tx-attach-fn req)]
                                           (let [target-id (g/override-root (:node-id item))
                                                 tx-data (tx-attach-fn target-id child-id)]
                                             (keep (fn [arc]
                                                     (let [src-serial-id (node-id->serial-id (gt/source-id arc))
                                                           tgt-serial-id (node-id->serial-id (gt/target-id arc))]
                                                       (when (and src-serial-id tgt-serial-id)
                                                         [src-serial-id (gt/source-label arc) tgt-serial-id (gt/target-label arc)])))
                                                   (g/tx-data-added-arcs tx-data)))))))
                             original-attachments)

        attachments (into []
                          (keep (fn [[parent-id child-id]]
                                  (let [parent-serial-id (node-id->serial-id parent-id)
                                        child-serial-id (node-id->serial-id child-id)]
                                    (when (and parent-serial-id child-serial-id)
                                      [parent-serial-id child-serial-id]))))
                          original-attachments)]
    (-> fragment
        (assoc :attachments attachments)
        (update :arcs (partial filterv #(not (contains? tx-attach-arcs %)))))))

(defn- default-copy-traverse [basis ^Arc arc]
  (let [src-node (.source-id arc)
        tgt-node (.target-id arc)
        tgt-label (.target-label arc)]
    (or (= :copied-nodes tgt-label)
        (and (or (= :child-outlines tgt-label)
                 (= :source-outline tgt-label))
             (g/node-instance? basis OutlineNode tgt-node)
             (not (and (g/node-instance? basis resource/ResourceNode src-node)
                       (some? (resource/path (g/node-value src-node :resource (g/make-evaluation-context {:basis basis :cache c/null-cache}))))))))))

(defn copy
  ([project src-item-iterators]
   (copy project src-item-iterators default-copy-traverse))
  ([project src-item-iterators traverse?]
   (let [root-ids (mapv #(:node-id (value %)) src-item-iterators)
         fragment (-> (g/copy root-ids {:traverse? traverse?
                                        :external-refs {project :project}
                                        :external-labels {project #{:collision-group-nodes
                                                                    :collision-groups-data
                                                                    :display-height
                                                                    :display-width
                                                                    :default-tex-params
                                                                    :settings}}})
                      (add-attachments root-ids))]

     (serialize fragment))))

(defn- read-only? [item-it]
  (:read-only (value item-it) false))

(defn- root? [item-iterator]
  (nil? (parent item-iterator)))

(defn delete? [item-iterators]
  (and (not-any? read-only? item-iterators)
       (not-any? root? item-iterators)))

(defn cut? [src-item-iterators]
  (and (delete? src-item-iterators)
    (loop [src-item-iterators src-item-iterators
           common-node-types nil]
     (if-let [item-it (first src-item-iterators)]
       (let [root-nodes [(g/node-by-id (:node-id (value item-it)))]
             parent (parent item-it)
             [_ reqs] (find-target-item parent root-nodes)
             node-types (set (map :node-type reqs))
             common-node-types (if common-node-types
                                 (set/intersection common-node-types node-types)
                                 node-types)]
         (if (and reqs (seq common-node-types))
           (recur (rest src-item-iterators) common-node-types)
           false))
       true))))

(defn cut!
  ([project src-item-iterators]
   (cut! project src-item-iterators []))
  ([project src-item-iterators extra-tx-data]
   (let [data (copy project src-item-iterators)
         root-ids (mapv #(:node-id (value %)) src-item-iterators)]
     (g/transact
       (concat
         (g/operation-label (localization/message "operation.cut"))
         (for [id root-ids]
           (g/delete-node (g/override-root id)))
         extra-tx-data))
     data)))

(defn- deserialize
  [text]
  (g/read-graph text (core/read-handlers)))

(defn- paste [project fragment]
  (g/paste (g/node-id->graph-id project) fragment {:external-refs {:project project}}))

(defn- nodes-by-id
  [paste-data]
  (->> (:tx-data paste-data)
       (g/tx-data-added-nodes)
       (coll/pair-map-by g/node-id)))

(defn- root-nodes [paste-data]
  (let [id->node (nodes-by-id paste-data)]
    (mapv (partial get id->node) (:root-node-ids paste-data))))

(defn- attach-pasted-nodes!
  [op-label op-seq item reqs root-node-ids]
  (assert (= (count reqs) (count root-node-ids)))
  (let [target (g/override-root (:node-id item))]
    ;; The tx-attach-fn will often look at the scope and assign unique names for
    ;; the pasted nodes. Performing individual transactions here means they will
    ;; get to see the names of the nodes that were pasted alongside them.
    (dorun
      (map (fn [node req]
             (when-some [tx-attach-fn (:tx-attach-fn req)]
               (g/transact
                 (concat
                   (g/operation-label op-label)
                   (g/operation-sequence op-seq)
                   (tx-attach-fn target node)))))
           root-node-ids
           reqs))))

(defn- do-paste!
  [op-label op-seq paste-data attachments item reqs select-fn]
  (let [serial-id->node-id (:serial-id->node-id paste-data)
        id->node (nodes-by-id paste-data)
        root-nodes (root-nodes paste-data)]
    (g/transact
      (concat
        (g/operation-label op-label)
        (g/operation-sequence op-seq)
        (:tx-data paste-data)))
    (attach-pasted-nodes! op-label op-seq item reqs (:root-node-ids paste-data))
    (doseq [[parent-serial-id child-serial-id] attachments]
      (let [parent-id (serial-id->node-id parent-serial-id)
            parent-outline (g/node-value parent-id :node-outline)
            child-id (serial-id->node-id child-serial-id)
            child-node (id->node child-id)
            [item reqs] (or (match-reqs [child-node] parent-outline)
                            (match-reqs [child-node] (:alt-outline parent-outline)))]
        (attach-pasted-nodes! op-label op-seq item reqs [child-id])))
    (when select-fn
      (g/transact
        (concat
          (g/operation-label op-label)
          (g/operation-sequence op-seq)
          (select-fn (mapv :_node-id root-nodes)))))))

(defn- paste-target [project item-iterator data]
  (let [paste-data (paste project (deserialize data))
        root-nodes (root-nodes paste-data)]
    (find-target-item item-iterator root-nodes)))

(defn paste! [project item-iterator data select-fn]
  (let [fragment (deserialize data)
        paste-data (paste project fragment)
        root-nodes (root-nodes paste-data)]
    (when-let [[item reqs] (find-target-item item-iterator root-nodes)]
      (do-paste! (localization/message "operation.paste") (gensym) paste-data (:attachments fragment) item reqs select-fn))))

(defn paste? [project item-iterator data]
  (try
    (some? (paste-target project item-iterator data))
    (catch Exception e
      (log/warn :exception e)
      ; TODO - ignore
      false)))

(defn drag? [item-iterators]
  (delete? item-iterators))

(defn- descendant? [src-item item-iterator]
  (if item-iterator
    (if (= src-item (value item-iterator))
      true
      (recur src-item (parent item-iterator)))
    false))

(defn drop? [project src-item-iterators item-iterator data]
  (and
    ; src is not descendant of target
    (not
      (reduce (fn [desc? it] (or desc?
                                 (descendant? (value it) item-iterator)))
              false src-item-iterators))
    ; pasting is allowed
    (when-let [[tgt _] (paste-target project item-iterator data)]
      (not (reduce (fn [parent? it]
                     (let [parent-item (value (parent it))]
                       (or parent? (= parent-item tgt) (= (:alt-outline parent-item) tgt)))) false src-item-iterators)))))

(defn drop! [project src-item-iterators item-iterator data select-fn]
  (when (drop? project src-item-iterators item-iterator data)
    (let [fragment (deserialize data)
          paste-data (paste project fragment)
          root-nodes (root-nodes paste-data)]
      (when-let [[item reqs] (find-target-item item-iterator root-nodes)]
        (let [op-seq (gensym)]
          (g/transact
            (concat
              (g/operation-label (localization/message "operation.drop"))
              (g/operation-sequence op-seq)
              (for [it src-item-iterators]
                (g/delete-node (g/override-root (:node-id (value it)))))))
          (do-paste! (localization/message "operation.drop") op-seq paste-data (:attachments fragment) item reqs select-fn))))))

(defn- trim-digits
  ^String [^String id]
  (loop [index (.length id)]
    (if (zero? index)
      ""
      (if (Character/isDigit (.charAt id (unchecked-dec index)))
        (recur (unchecked-dec index))
        (subs id 0 index)))))

(defn name-resource-pairs [taken-ids resources]
  (let [names (id/resolve-all (map resource/base-name resources) taken-ids)]
    (map vector names resources)))

(defn gen-node-outline-keys [prefixes]
  (loop [prefixes prefixes
         counts-by-prefix {}
         node-outline-keys []]
    (if-some [prefix (first prefixes)]
      (let [^long count (get counts-by-prefix prefix 0)]
        (assert (string? (not-empty prefix)))
        (recur (next prefixes)
               (assoc counts-by-prefix prefix (inc count))
               (conj node-outline-keys (str prefix count))))
      node-outline-keys)))

(defn taken-node-outline-keys
  ([parent-outline-node]
   (g/with-auto-evaluation-context evaluation-context
     (taken-node-outline-keys parent-outline-node evaluation-context)))
  ([parent-outline-node evaluation-context]
   (into #{}
         (keep :node-outline-key)
         (:children (g/node-value parent-outline-node :node-outline evaluation-context)))))

(defn next-node-outline-key [template-node-outline-key taken-node-outline-keys]
  ;; Contrary to resolve-id, we want to return the next id following the highest
  ;; existing id. We don't want added nodes to reuse freed slots.
  (let [prefix (trim-digits template-node-outline-key)
        next-index (inc (transduce (comp (filter #(string/starts-with? % prefix))
                                         (map #(subs % (count prefix)))
                                         (map #(Long/parseUnsignedLong %)))
                                   max
                                   -1
                                   taken-node-outline-keys))]
    (str prefix next-index)))

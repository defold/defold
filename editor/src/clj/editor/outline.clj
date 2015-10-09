(ns editor.outline
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.project :as project]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [editor.resource FileResource ZipResource]))

(defprotocol ItemIterator
  (value [this])
  (parent [this]))

(defn- node-instance? [type node]
  (let [node-type (g/node-type node)
        all-types (into #{node-type} (g/supertypes node-type))]
    (all-types type)))

(defn- req-satisfied? [req node]
  (and (node-instance? (:node-type req) node)
       (reduce (fn [v [field valid-fn]] (and v (valid-fn (get node field)))) true (:values req))))

(defn- find-req [node reqs]
  (when reqs
    (first (filter #(req-satisfied? % node) reqs))))

(defn- serialize
  [fragment]
  (g/write-graph fragment (core/write-handlers)))

(defn- match-reqs [target-item root-nodes]
  (let [all-reqs (:child-reqs target-item)]
    (loop [root-nodes root-nodes
           matched-reqs []]
      (if-let [node (first root-nodes)]
        (if-let [match (find-req node all-reqs)]
          (recur (rest root-nodes) (conj matched-reqs match))
          nil)
        matched-reqs))))

(defn- find-target-item [item-iterator root-nodes]
  (if item-iterator
    (let [item (value item-iterator)]
      (if-let [reqs (match-reqs item root-nodes)]
        [item reqs]
        (recur (parent item-iterator) root-nodes)))
    nil))

(defrecord ResourceReference [path])

(core/register-record-type! ResourceReference)

(defn file-node?
  [node]
  (and (g/node-instance? project/ResourceNode (g/node-id node))
       (or (instance? FileResource (:resource node))
           (instance? ZipResource  (:resource node)))))

(defn- actual-path [node]
  (workspace/proj-path (:resource node)))

(defn- make-reference [node]
  (when-let [project-path (actual-path node)]
    (ResourceReference. project-path)))

(defn- resolve-reference [basis project reference]
  (let [resource (workspace/resolve-workspace-resource (project/workspace project) (:path reference))]
     (g/node-by-id-at basis (project/get-resource-node project resource))))

(defn copy [src-item-iterators]
  (let [root-ids (mapv #(:node-id (value %)) src-item-iterators)
        fragment (g/copy root-ids {:include?  (comp not file-node?)
                                   :serializer (fn [node]
                                                 (if (file-node? node)
                                                   (make-reference node)
                                                   (g/default-node-serializer node)))})]
    (serialize fragment)))

(defn cut? [src-item-iterators]
  (loop [src-item-iterators src-item-iterators]
    (if-let [item-it (first src-item-iterators)]
      (let [root-nodes [(g/node-by-id (:node-id (value item-it)))]
            parent (parent item-it)]
        (if (find-target-item parent root-nodes)
          (recur (rest src-item-iterators))
          false))
      true)))

(defn cut! [src-item-iterators]
  (let [data     (copy src-item-iterators)
        root-ids (mapv #(:node-id (value %)) src-item-iterators)]
    (g/transact
      (concat
        (g/operation-label "Cut")
        (for [id root-ids]
          (g/delete-node id))))
    data))

(defn- deserialize
  [text]
  (g/read-graph text (core/read-handlers)))

(defn- paste [project fragment]
  (let [graph (project/graph project)]
    (g/paste graph (deserialize fragment)
             {:deserializer
              (fn [basis graph-id record]
                (if (instance? ResourceReference record)
                  (resolve-reference basis project record)
                  (g/default-node-deserializer basis graph-id record)))})))

(defn- root-nodes [paste-data]
  (let [nodes (into {} (map #(let [n (:node %)] [(:_node-id n) n]) (filter #(= (:type %) :create-node) (:tx-data paste-data))))]
    (mapv (partial get nodes) (:root-node-ids paste-data))))

(defn- build-tx-data [item reqs paste-data]
  (let [target (:node-id item)]
    (concat
      (:tx-data paste-data)
      (for [[node req] (map vector (:root-node-ids paste-data) reqs)]
        (if-let [tx-attach-fn (:tx-attach-fn req)]
          (tx-attach-fn target node)
          [])))))

(defn paste! [project item-iterator data]
  (let [paste-data (paste project data)
        root-nodes (root-nodes paste-data)]
        (when-let [[item reqs] (find-target-item item-iterator root-nodes)]
          (g/transact
            (concat
              (g/operation-label "Paste")
              (build-tx-data item reqs paste-data)
              (project/select project (mapv :_node-id root-nodes)))))))

(defn paste? [project item-iterator data]
  (try
    (let [paste-data (paste project data)
         root-nodes (root-nodes paste-data)]
     (some? (find-target-item item-iterator root-nodes)))
    (catch Exception e
      (log/warn :exception e)
      ; TODO - ignore
      false)))

(defn- root? [item-iterator]
  (nil? (parent item-iterator)))

(defn drag? [project item-iterators]
  (not-any? root? item-iterators))

(defn- descendant? [src-item item-iterator]
  (if item-iterator
    (if (= src-item (value item-iterator))
      true
      (recur src-item (parent item-iterator)))
    false))

(defn drop? [project src-item-iterators item-iterator data]
  (and
    ; target is not parent of source
    (let [tgt (value item-iterator)]
      (not (reduce (fn [parent? it] (or parent? (= (value (parent it)) tgt))) false src-item-iterators)))
    ; src is not descendant of target
    (not
      (reduce (fn [desc? it] (or desc?
                                 (descendant? (value it) item-iterator)))
              false src-item-iterators))
    ; pasting is allowed
    (paste? project item-iterator data)))

(defn drop! [project src-item-iterators item-iterator data]
  (when (drop? project src-item-iterators item-iterator data)
    (let [paste-data (paste project data)
          root-nodes (root-nodes paste-data)]
      (when-let [[item reqs] (find-target-item item-iterator root-nodes)]
        (g/transact
          (concat
            (g/operation-label "Drop")
            (build-tx-data item reqs paste-data)
            (for [it src-item-iterators]
              (g/delete-node (:node-id (value it))))
            (project/select project (mapv :_node-id root-nodes))))))))

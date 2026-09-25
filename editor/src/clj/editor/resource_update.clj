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

(ns editor.resource-update
  "Translates a resource_watch/diff to a change plan for corresponding resource nodes.

  The change plan produced by resource-change-plan below has the following keys:
  * new - a list of resources to create new resource nodes for
  * delete - a list of resource nodes to remove - either because they were really removed or because they have been replaced
  * transfer-overrides - a list of pairs of new resource & old node. The resource should be in :new, and any override nodes that used to have old node as override-original will be transferred to the newly created resource node.
  * transfer-outgoing-arcs - a list of pairs resource & list of [label [target node, target label]]. The connections will be added to the resource node corresponding to the resource.
  * mark-deleted - a list of resource nodes to be marked as deleted
  * invalidate-outputs - a list of resource nodes (stateless) whose outputs should be invalidated
  * redirect - a list of [resource node, new resource] pairs, the resource nodes :resource will be reset to new resource."
  (:require [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

(defn print-plan [plan]
  (let [resource-info (fn [r] (pr-str r))
        node-type-name (fn [node] (:name @(g/node-type* node)))
        resource-node-info (fn [node] (str "{:" (node-type-name node) " " node
                                           (when (g/override? node) (str " :override " (g/override-original node)))
                                           " :resource " (resource-info (g/node-value node :resource)) "}"))
        generic-node-info (fn [node] (str "{:" (node-type-name node) " " node
                                          (when (g/override? node) (str " :override " (g/override-original node))) "}"))]

    (doseq [kept (:kept plan)]
      (println "KEPT:" (resource-info kept)))
    (doseq [new (:new plan)]
      (println "NEW:" (resource-info new)))
    (doseq [[resource old-node] (:transfer-overrides plan)]
      (println "TRANSFER OVERRIDE FROM:" (resource-node-info old-node) "TO NEW NODE FOR:" (resource-info resource)))
    (doseq [[resource explicit-outputs] (:transfer-outgoing-arcs plan)]
      (println "TRANSFER ARCS TO NEW NODE FOR:" (resource-info resource))
      (doseq [[source-label [target-node target-label]] explicit-outputs]
        (println "  " source-label "->" (str target-label " " (generic-node-info target-node)))))
    (doseq [deleted (:delete plan)]
      (println "DELETE:" deleted (resource-node-info deleted)))
    (doseq [delete-marked (:mark-deleted plan)]
      (println "MARK DELETED:" (resource-node-info delete-marked)))
    (doseq [invalidate (:invalidate-outputs plan)]
      (println "INVALIDATE OUTPUTS:" (resource-node-info invalidate)))
    (doseq [[node new] (:redirect plan)]
      (println "REDIRECTING:" (resource-node-info node) "TO:" (resource-info new))))
  plan)

(defn- merge-resource-change-plans [& plans]
  (apply merge-with into plans))

(defn- keep-existing-node?
  "Check if we can safely keep the existing node in the graph. Every time a file
  has an updated timestamp it will be reported as changed, but if the contents
  are unchanged we don't need to reload the node. The old node must have
  registered a disk-sha256 using the workspace/set-disk-sha256 function for the
  existing node to potentially be kept. Resource nodes will typically call
  workspace/set-disk-sha256 from their load-fn, but lazy-loaded resource nodes
  can use other methods."
  [old-node new-resource old-node->old-disk-sha256]
  (if-some [old-disk-sha256 (old-node->old-disk-sha256 old-node)]
    (if-some [new-disk-sha256 (try
                                (resource/resource->sha256-hex new-resource)
                                (catch Exception _
                                  nil))]
      (= old-disk-sha256 new-disk-sha256)
      false)
    false))

(defn- replace-resources-plan [{:keys [resource->old-node old-node->old-disk-sha256]} resources force-replacement]
  ;; Creates new resource nodes for all resources, transfers overrides and outgoing arcs
  ;; from old (if any) and deletes them. If force-replacement is false, we check whether
  ;; the file has significant changes before replacing any nodes.
  (let [in-graph (into []
                       (keep (fn [resource]
                               (when-some [old-node (resource->old-node resource)]
                                 [resource old-node])))
                       resources)
        [kept replaced] (if force-replacement
                          [nil in-graph]
                          (coll/separate-by (fn [[resource old-node]]
                                              (keep-existing-node? old-node resource old-node->old-disk-sha256))
                                            in-graph))
        transfer-overrides replaced
        transfer-outgoing-arcs (mapv (fn [[resource old-node]]
                                       [resource (gu/explicit-outputs old-node)])
                                     replaced)
        delete (mapv second replaced)
        kept-resources (into #{} (map first) kept)
        new-resources (into [] (remove kept-resources) resources)]
    {:new new-resources
     :transfer-overrides transfer-overrides
     :transfer-outgoing-arcs transfer-outgoing-arcs
     :delete delete
     :kept kept-resources}))

(defn- resource-added-plan [{:keys [added]} {:keys [move-target-paths] :as plan-info}]
  (let [non-moved-added (remove (comp move-target-paths resource/proj-path) added)]
    ;; When a non-existing resource is referenced, a dummy "missing file" node is added to
    ;; the graph and marked defective, so we must attempt a full replacement here rather
    ;; than simply adding new nodes for the introduced resources.
    (replace-resources-plan plan-info non-moved-added true)))

(defn- resource-removed-plan [{:keys [removed]} {:keys [move-source-paths resource->old-node]}]
  ;; You might think that we should only have to invalidate-outputs for
  ;; stateless (external) resources, and the next attempt to access the data
  ;; would cause an appropriate "file not found" error. However, we actually
  ;; want the node to be marked defective, as we specifically check for this in
  ;; some places. For example, double-clicking a build error will navigate to
  ;; the referencing resource rather than the missing resource when the missing
  ;; resource node is marked defective with a :file-not-found error type.
  ;;
  ;; Marking stateless nodes as defective when their resources are deleted is
  ;; also consistent with what happens to them when we load the project.
  ;;
  ;; The additional complexities around ZipResources detailed in the comment in
  ;; the resource-changed-plan function below also apply.
  ;;
  ;; Note that marking a node as deleted will implicitly invalidate its cached
  ;; outputs, so no need to emit :invalidate-outputs instructions here.
  (let [non-moved-removed (remove (comp move-source-paths resource/proj-path) removed)]
    {:mark-deleted (mapv resource->old-node non-moved-removed)}))

(defn- resource-changed-plan [{:keys [changed]}
                              {:keys [move-source-paths
                                      move-target-paths
                                      resource->old-node
                                      resource-swapped?]
                               :as plan-info}]
  ;; Ideally, for stateless (external) resources, we should only have to invalidate-outputs
  ;; and the data will be reloaded from disk when needed (through FileResource).
  ;; But in some cases the actual resource/Resource *value* is changed, from one
  ;; ZipResource to another - e.g. if you replace a library dependency with another
  ;; one that has the same file - thus, we must update the :resource field of the
  ;; node. Just like a redirect.
  (let [non-moved-changed (remove (comp (some-fn move-source-paths move-target-paths) resource/proj-path) changed)
        {loadable-changed true stateless-changed false} (group-by resource/stateful? non-moved-changed)
        {stateless-swapped true stateless-changed false} (group-by resource-swapped? stateless-changed)]
    (merge-resource-change-plans
      (replace-resources-plan plan-info loadable-changed false)
      {:invalidate-outputs (mapv resource->old-node stateless-changed)
       :redirect (mapv (juxt resource->old-node identity) stateless-swapped)})))

(defmulti resource-moved-case-plan (fn [case _ _] case))

(defmethod resource-moved-case-plan [:removed :added] [_ move-pairs {:keys [resource->old-node]}]
  ;; For added target resources we can still have a lingering mark-defectiv'ed/invalidate-output'ed node from a
  ;; previously deleted version of the resource. We transfer all connections & overrides from the lingerers
  ;; to the to-be-redirected resource before deleting them.
  (let [redirects (mapv (fn redirects-selector [[source target]]
                          [(resource->old-node source) target])
                        move-pairs)
        transfer-overrides (into []
                                 (keep (fn transfer-overrides-selector [[source target]]
                                         (when-some [old-target-node (resource->old-node target)]
                                           [source old-target-node])))
                                 move-pairs)
        transfer-outgoing-arcs (mapv (fn transfer-outgoing-arcs-selector [[source old-target-node]]
                                       [source (gu/explicit-outputs old-target-node)])
                                     transfer-overrides)
        delete (into []
                     (keep (fn delete-selector [[_ target]]
                             (resource->old-node target)))
                     move-pairs)]
    {:transfer-overrides transfer-overrides
     :transfer-outgoing-arcs transfer-outgoing-arcs
     :delete delete
     :redirect redirects}))

(defmethod resource-moved-case-plan [:removed :changed] [_ move-pairs {:keys [resource->old-node]}]
  ;; We don't have to do the resource->changed-resource translation because move target always comes from new snapshot.
  (let [{loadable-targets-changed true stateless-targets-changed false} (group-by resource/stateful? (map second move-pairs))
        ;; Transfer overrides from old source nodes to the new target node, and from the old loadable target nodes.
        ;; We don't need to bother with stateless targets as those nodes are not replaced.
        transfer-overrides (into (mapv (fn [[source target]] [target (resource->old-node source)]) move-pairs)
                                 (mapv (fn [target] [target (resource->old-node target)]) loadable-targets-changed))
        ;; Transfer arcs from old source nodes to the new target node, and from the old loadable target nodes.
        ;; We dont need to bother with stateless targets as those nodes are not replaced.
        transfer-outgoing-arcs (into (mapv (fn [[source target]] [target (gu/explicit-outputs (resource->old-node source))]) move-pairs)
                                     (mapv (fn [target] [target (gu/explicit-outputs (resource->old-node target))]) loadable-targets-changed))
        delete (mapv resource->old-node (concat (map first move-pairs) loadable-targets-changed))]
    {:new (vec loadable-targets-changed)
     :transfer-overrides transfer-overrides
     :transfer-outgoing-arcs transfer-outgoing-arcs
     :delete delete
     :invalidate-outputs (mapv resource->old-node stateless-targets-changed)}))

(defmethod resource-moved-case-plan [:changed :added] [_ move-pairs {:keys [resource->old-node resource-swapped? resource->changed-resource]}]
  ;; Just like in resource-changed-plan, we must handle the case of the actual resource value being updated
  ;; for stateless resources. I.e. redirect instead of invalidate-outputs.
  (let [{loadable-sources-changed true stateless-sources-changed false} (group-by resource/stateful? (map (comp resource->changed-resource first) move-pairs))
        new (concat loadable-sources-changed (map second move-pairs))
        transfer-overrides (into (mapv (fn [source] [source (resource->old-node source)]) loadable-sources-changed)
                                 (keep (fn [[_ target]] (when-let [old-target-node (resource->old-node target)] [target old-target-node])))
                                 move-pairs)
        transfer-outgoing-arcs (into (mapv (fn [source] [source (gu/explicit-outputs (resource->old-node source))]) loadable-sources-changed)
                                     (keep (fn [[_ target]] (when-let [old-target-node (resource->old-node target)] [target (gu/explicit-outputs old-target-node)])))
                                     move-pairs)
        delete (into [] (keep resource->old-node) (concat loadable-sources-changed (map second move-pairs)))
        {stateless-swapped true stateless-changed false} (group-by resource-swapped? stateless-sources-changed)]
    {:new (vec new)
     :transfer-overrides transfer-overrides
     :transfer-outgoing-arcs transfer-outgoing-arcs
     :delete delete
     :invalidate-outputs (mapv resource->old-node stateless-changed)
     :redirect (mapv (juxt resource->old-node identity) stateless-swapped)}))

(defmethod resource-moved-case-plan [:changed :changed] [_ move-pairs {:keys [resource->old-node resource-swapped? resource->changed-resource]}]
  ;; Just like in resource-changed-plan, we must handle the case of the actual resource value being updated
  ;; for stateless resources. I.e. redirect instead of invalidate-outputs.
  (let [{loadable-sources-changed true stateless-sources-changed false} (group-by resource/stateful? (map (comp resource->changed-resource first) move-pairs))
        {loadable-targets-changed true stateless-targets-changed false} (group-by resource/stateful? (map (comp resource->changed-resource second) move-pairs))
        new (concat loadable-sources-changed loadable-targets-changed)
        transfer-overrides (into (mapv (fn [source] [source (resource->old-node source)]) loadable-sources-changed)
                                 (mapv (fn [target] [target (resource->old-node target)]) loadable-targets-changed))
        transfer-outgoing-arcs (into (mapv (fn [source] [source (gu/explicit-outputs (resource->old-node source))]) loadable-sources-changed)
                                     (mapv (fn [target] [target (gu/explicit-outputs (resource->old-node target))]) loadable-targets-changed))
        delete (mapv resource->old-node (concat loadable-sources-changed loadable-targets-changed))
        stateless-changed (concat stateless-sources-changed stateless-targets-changed)
        {stateless-swapped true stateless-changed false} (group-by resource-swapped? stateless-changed)]
    {:new (vec new)
     :transfer-overrides transfer-overrides
     :transfer-outgoing-arcs transfer-outgoing-arcs
     :delete delete
     :invalidate-outputs (mapv resource->old-node stateless-changed)
     :redirect (mapv (juxt resource->old-node identity) stateless-swapped)}))

(defn- resource-moved-plan [{:keys [added removed changed moved] :as changes} plan-info]
  (let [added-paths (into #{} (map resource/proj-path) added)
        removed-paths (into #{} (map resource/proj-path) removed)
        changed-paths (into #{} (map resource/proj-path) changed)
        resource-status (comp (fn [resource-path]
                                ;; Since we only use mtime to determine if a resource has changed,
                                ;; it could be that the move target ends up in neither added nor
                                ;; changed.
                                ;; For instance. Copy particle_blob.png from builtins to the project
                                ;; root. Make a temp dir, copy particle_blob.png there.
                                ;; Now, move the /particle_blob.png to the temp dir. The source will
                                ;; likely be in removed. But since the mtime is typically copied/moved along,
                                ;; the target (overwritten) particle_blob.png in the temp dir does _not_
                                ;; end up in changed.
                                ;; Similarly, a library resource suddenly being loaded in place of the
                                ;; source resource could (very unlikely but...) accidentally have the same
                                ;; mtime - and so end up in neither removed nor changed.
                                ;; A pessimistic workaround for this is to treat all such resources
                                ;; as changed and subsequently reload/invalidate them.
                                (cond
                                  (added-paths resource-path) :added
                                  (removed-paths resource-path) :removed
                                  (changed-paths resource-path) :changed
                                  :else :changed))
                              resource/proj-path)
        move-pair-status (fn [[source target]] [(resource-status source) (resource-status target)])
        move-cases (group-by move-pair-status moved)]
    (apply merge-resource-change-plans
           (map (fn [[case move-pairs]]
                  (resource-moved-case-plan case move-pairs plan-info))
                move-cases))))

(defn- exclude-extension-changes [moved]
  ;; It's possible to add an extension to file when renaming it, e.g.,
  ;; /foo -> /foo.lua
  ;; We should not redirect such nodes, because different resource extensions
  ;; might imply different resource node types
  (filterv #(= (resource/type-ext (first %)) (resource/type-ext (second %))) moved))

(defn resource-change-plan [old-nodes-by-path old-node->old-disk-sha256 changes]
  (let [changes (update changes :moved exclude-extension-changes)
        {:keys [added removed changed moved]} changes
        basis (g/now)
        move-sources (map first moved)
        move-targets (map second moved)
        move-source-paths (into #{} (map resource/proj-path) move-sources)
        move-target-paths (into #{} (map resource/proj-path) move-targets)
        resource->old-node (comp old-nodes-by-path resource/proj-path)
        old-resource-by-old-node (into {}
                                       (map (fn [[_ old-node]]
                                              (let [old-resource (resource-node/resource basis old-node)]
                                                [old-node old-resource])))
                                       old-nodes-by-path)
        resource->old-resource (comp old-resource-by-old-node resource->old-node)
        resource-swapped? (fn [r] (not= r (resource->old-resource r)))
        changed-map (into {} (map (juxt resource/proj-path identity)) changed)
        resource->changed-resource (some-fn (comp changed-map resource/proj-path) identity) ; fallback to original
        plan-info {:move-source-paths move-source-paths
                   :move-target-paths move-target-paths
                   :resource->old-node resource->old-node
                   :resource->old-resource resource->old-resource
                   :resource-swapped? resource-swapped?
                   :resource->changed-resource resource->changed-resource
                   :old-node->old-disk-sha256 old-node->old-disk-sha256}]
    ;; sanity checks
    (let [known? (fn [r] (contains? old-nodes-by-path (resource/proj-path r)))
          unknown? (complement known?)
          unknown-changed (filter unknown? changed)
          unknown-removed (filter unknown? removed)
          unknown-move-sources (filter unknown? move-sources)]
      (assert (empty? unknown-changed)
              (mapv resource/proj-path unknown-changed)) ; no unknown resources are :changed
      (assert (empty? unknown-removed)
              (mapv resource/proj-path unknown-removed)) ; no unknown resources have been :removed
      (assert (empty? unknown-move-sources)
              (mapv resource/proj-path unknown-move-sources))) ; no unknown resources have been :moved
    (let [plan (merge-resource-change-plans (resource-added-plan changes plan-info)
                                            (resource-removed-plan changes plan-info)
                                            (resource-changed-plan changes plan-info)
                                            (resource-moved-plan changes plan-info))]
      (doseq [[_ v] plan]
        ;; We should never produce lazy lists.
        (assert (or (vector? v)
                    (set? v))))
      plan)))

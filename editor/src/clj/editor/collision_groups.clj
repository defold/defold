(ns editor.collision-groups
  "Manages allocation of stable and limited ids to collision-groups."
  (:require
   [clojure.set :as set]
   [dynamo.graph :as g]
   [editor.colors :as colors]))

(def MAX-GROUPS 16)

(def empty-state
  {;; which group has which id
   :group->id  {}

   ;; which node uses which group
   :node->group {}

   ;; which ids are available
   :available-ids (into (clojure.lang.PersistentQueue/EMPTY) (range MAX-GROUPS))})


(defn- make-node-set->group
  [node->group]
  (reduce-kv (fn [ret group nodes]
               (assoc ret (set (map first nodes)) group))
             {}
             (group-by val node->group)))

(defn- renamed-groups
  [old-node->group new-node->group]
  (let [old-node-set->group (make-node-set->group old-node->group)
        new-node-set->group (make-node-set->group new-node->group)]
    (->> (keys new-node-set->group)
         (keep (fn [node-ids]
                 (let [old-group (old-node-set->group node-ids)
                       new-group (new-node-set->group node-ids)]
                   (when (and old-group new-group (not= old-group new-group))
                     [old-group new-group]))))
         (into {}))))

(defn- free-unused-ids
  [state removed-groups]
  (reduce (fn [{:keys [group->id] :as state} collision-group]
            (-> state
                (update :group->id dissoc collision-group)
                (update :available-ids conj (group->id collision-group))))
          state
          removed-groups))

(defn- retain-renamed-ids
  [state renamed-groups]
  (reduce-kv (fn [{:keys [group->id] :as state} old-name new-name]
               (-> state
                   (update :group->id dissoc old-name)
                   (update :group->id assoc new-name (group->id old-name))))
             state
             renamed-groups))

(defn- allocate-new-ids
  [state new-groups collision-group-nodes]
  (reduce (fn [{:keys [group->id available-ids] :as state} {:keys [collision-group]}]
            (if (and (new-groups collision-group)
                     (not (group->id collision-group)))
              (if-let [id (first available-ids)]
                (-> state
                    (update :group->id assoc collision-group id)
                    (update :available-ids pop))
                state)
              state))
          state
          (sort-by :node-id collision-group-nodes)))

(defn- allocate-ids
  [{:keys [node->group group->id available-ids] :as state} collision-group-nodes]
  (let [new-node->group      (into {} (map (juxt :node-id :collision-group) collision-group-nodes))
        old-groups           (set (vals node->group))
        new-groups           (set (vals new-node->group))
        renamed-groups       (renamed-groups node->group new-node->group)
        removed-groups       (set/difference old-groups new-groups (keys renamed-groups))
        added-groups         (set/difference new-groups old-groups (vals renamed-groups))]
    (-> {:group->id group->id
         :node->group new-node->group
         :available-ids available-ids}
        (retain-renamed-ids renamed-groups)
        (free-unused-ids removed-groups)
        (allocate-new-ids added-groups collision-group-nodes))))

(defn allocate-collision-groups
  [state collision-group-nodes]
  (allocate-ids state collision-group-nodes))


(def idx->hue-offset
  (let [step (/ MAX-GROUPS 4)]
    (vec (for [n (range MAX-GROUPS)]
           (let [col (* n step)
                 row (long (Math/floor (/ col MAX-GROUPS)))]
             (mod (+ col row) MAX-GROUPS))))))

(defn hue
  [collision-groups-state collision-group]
  (when-let [id (get-in collision-groups-state [:group->id collision-group])]
    (* (/ 360.0 MAX-GROUPS) (nth idx->hue-offset id))))

(defn color
  [collision-groups-state collision-group]
  (if-let [hue (hue collision-groups-state collision-group)]
    (colors/hsl->rgba hue 1.0 0.75)
    [1.0 1.0 1.0 1.0]))

(defn node->group
  [collision-groups-state collision-group-node]
  (get-in collision-groups-state [:node->group collision-group-node]))

(defn node->color
  [collision-groups-state collision-group-node]
  (color collision-groups-state (node->group collision-groups-state collision-group-node)))

(defn collision-groups
  [collision-groups-state]
  (-> collision-groups-state :group->id keys))

(defn overallocated?
  [collision-groups-state]
  (< (-> collision-groups-state :group->id count)
     (-> collision-groups-state :node->group vals set count)))

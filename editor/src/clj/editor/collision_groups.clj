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

(ns editor.collision-groups
  "Manages allocation of stable and limited ids to collision-groups."
  (:require [editor.colors :as colors]))

(def MAX-GROUPS 16)

(defn make-collision-groups-data
  [collision-group-nodes]
  (let [node->group (into {} (mapv (juxt :node-id :collision-group) collision-group-nodes))]
    (reduce (fn [{:keys [group->id node->group] :as data} {:keys [node-id collision-group]}]
              (let [ngroups (count group->id)]
                (if (or (group->id collision-group)
                        (<= MAX-GROUPS ngroups))
                  data
                  (assoc-in data [:group->id collision-group] ngroups))))
            {:group->id {}
             :node->group node->group}
            (sort-by :node-id collision-group-nodes))))

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

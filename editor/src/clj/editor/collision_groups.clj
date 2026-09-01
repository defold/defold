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
  (:require [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.localization :as localization]
            [util.fn :as fn]
            [util.murmur :as murmur]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:const MAX-GROUPS 16)

(def ^:private more-than-max-collision-groups-in-use-message (localization/message "error.tile-source.more-than-max-collision-groups-in-use" {"max" MAX-GROUPS}))

(defn- color-raw
  [collision-group]
  (if-not collision-group
    colors/white
    (let [hash (murmur/hash64 collision-group)
          group-index (mod hash MAX-GROUPS)
          hue (* (/ 360.0 MAX-GROUPS)
                 (double group-index))]
      (colors/hsl->rgba hue 1.0 0.75))))

(def color (fn/memoize color-raw))

(defn overallocated?
  [collision-groups]
  (< MAX-GROUPS
     (count (set collision-groups))))

(defn validate
  [collision-groups]
  (when (overallocated? collision-groups)
    (g/map->error
      {:severity :warning
       :message more-than-max-collision-groups-in-use-message})))

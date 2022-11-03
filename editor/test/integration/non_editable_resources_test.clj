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

(ns integration.non-editable-resources-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.math :as math]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.workspace :as workspace]
            [integration.test-util :as tu]
            [internal.util :as util]
            [support.test-support :refer [with-clean-system]]
            [util.coll :refer [pair]]
            [util.digestable :as digestable]))

(set! *warn-on-reflection* true)

(defn- class-keyword [value]
  (-> value class .getSimpleName keyword))

(defn- class-symbol [value]
  (-> value class .getName symbol))

(defn- extra-comparable-resource-data [resource]
  (cond
    (workspace/build-resource? resource)
    (recur (:resource resource))

    (contains? resource :data)
    {:data (:data resource)}))

(defn- comparable-build-targets [build-targets]
  (letfn [(value-fn [key value]
            (cond
              (resource/resource? value)
              (merge (sorted-map (class-keyword value) (resource/proj-path value))
                     (extra-comparable-resource-data value))

              (satisfies? math/VecmathConverter value)
              {(class-keyword value) (math/vecmath->clj value)}

              (record? value)
              (into (sorted-map :- (class-symbol value))
                    (map (fn [[k v]]
                           (let [v' (util/deep-map-kv-helper identity value-fn k v)]
                             (pair k v'))))
                    value)

              (fn? value)
              {:Fn (digestable/fn->symbol value)}

              (instance? com.google.protobuf.ByteString value)
              {:ByteString (into (vector-of :byte) value)}

              (digestable/node-id-entry? key value)
              nil ; Exclude the entry.

              :else
              value))]
    (util/deep-map-kv util/with-sorted-keys value-fn build-targets)))

(defn create-project! [world project-path atlas-proj-paths]
  (let [workspace (tu/setup-workspace! world project-path)
        project (tu/setup-project! workspace)]
    (tu/make-atlas-resource-node! project "/assets/from-script.atlas")
    (tu/make-atlas-resource-node! project "/assets/from-chair-sprite.atlas")
    (tu/make-atlas-resource-node! project "/assets/from-room-embedded-chair-sprite.atlas")
    (doseq [atlas-proj-path atlas-proj-paths]
      (tu/make-atlas-resource-node! project atlas-proj-path))
    (let [sprite-resource-type (workspace/get-resource-type workspace "sprite")
          script (doto (tu/make-resource-node! project "/assets/script.script")
                         (tu/code-editor-source! "go.property('atlas', resource.atlas('/assets/from-script.atlas'))"))
          chair (tu/make-resource-node! project "/assets/chair.go")
          chair-script (tu/add-referenced-component! chair (resource-node/resource script))
          chair-sprite (tu/add-embedded-component! chair sprite-resource-type)
          room (tu/make-resource-node! project "/assets/room.collection")
          room-embedded-chair (tu/add-embedded-game-object! room)
          room-embedded-chair-script (tu/add-referenced-component! room-embedded-chair (resource-node/resource script))
          room-embedded-chair-sprite (tu/add-embedded-component! room-embedded-chair sprite-resource-type)
          room-referenced-chair (tu/add-referenced-game-object! room (resource-node/resource chair))
          room-referenced-chair-script (tu/referenced-component room-referenced-chair)
          house (tu/make-resource-node! project "/assets/house.collection")
          house-room (tu/add-referenced-collection! house (resource-node/resource room))
          house-room-embedded-chair (tu/embedded-game-object house-room)
          house-room-embedded-chair-script (tu/referenced-component house-room-embedded-chair)
          house-room-referenced-chair (tu/referenced-game-object house-room)
          house-room-referenced-chair-script (tu/referenced-component house-room-referenced-chair)]
      (is (not (g/override? chair-script)))
      (is (not (g/override? room-embedded-chair-script)))
      (is (= chair-script (g/override-original room-referenced-chair-script)))
      (is (= room-embedded-chair-script (g/override-original house-room-embedded-chair-script)))
      (is (= room-referenced-chair-script (g/override-original house-room-referenced-chair-script)))
      (g/transact
        (concat
          (g/set-property chair-script :id "chair-script")
          (g/set-property chair-sprite :id "chair-sprite")
          (g/set-property room-embedded-chair :id "room-embedded-chair")
          (g/set-property room-embedded-chair-script :id "room-embedded-chair-script")
          (g/set-property room-embedded-chair-sprite :id "room-embedded-chair-sprite")
          (g/set-property room-referenced-chair :id "room-referenced-chair")
          (g/set-property house-room :id "house-room")))
      (tu/prop! chair-sprite :image (tu/resource workspace "/assets/from-chair-sprite.atlas"))
      (tu/prop! chair-sprite :default-animation "from-chair-sprite")
      (tu/prop! room-embedded-chair-sprite :image (tu/resource workspace "/assets/from-room-embedded-chair-sprite.atlas"))
      (tu/prop! room-embedded-chair-sprite :default-animation "from-room-embedded-chair-sprite")
      {:chair chair
       :chair-script chair-script
       :house house
       :house-room-embedded-chair-script house-room-embedded-chair-script
       :house-room-referenced-chair-script house-room-referenced-chair-script
       :project project
       :room room
       :room-embedded-chair-script room-embedded-chair-script
       :room-referenced-chair-script room-referenced-chair-script
       :script script
       :workspace workspace})))

(defn- load-project! [world project-path]
  (let [workspace (tu/setup-workspace! world project-path)
        project (tu/setup-project! workspace)
        chair (tu/resource-node project "/assets/chair.go")
        room (tu/resource-node project "/assets/room.collection")
        house (tu/resource-node project "/assets/house.collection")]
    {:chair chair
     :house house
     :project project
     :room room
     :workspace workspace}))

(defn- save-project! [project]
  (let [save-data (project/dirty-save-data project)]
    (project/write-save-data-to-disk! save-data nil)
    (project/invalidate-save-data-source-values! save-data)))

(defn- set-non-editable-directories! [project-root-path non-editable-directory-proj-paths]
  (shared-editor-settings/write-config!
    project-root-path
    (cond-> {}
            (seq non-editable-directory-proj-paths)
            (assoc :non-editable-directories (vec non-editable-directory-proj-paths)))))

(defn- perform-non-editable-resources-test [atlas-property-proj-paths-by-node-key]
  (let [project-path (tu/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (tu/make-directory-deleter project-path)]
      ;; Populate the project and capture output from the resource nodes while
      ;; they are in an editable state. Then mark the resources as non-editable.
      (let [editable-results
            (with-clean-system
              (let [atlas-proj-paths (into (sorted-set) (vals atlas-property-proj-paths-by-node-key))
                    {:keys [chair
                            house
                            project
                            room
                            workspace] :as editable-node-ids-by-key} (create-project! world project-path atlas-proj-paths)]
                (doseq [[node-key atlas-proj-path] atlas-property-proj-paths-by-node-key]
                  (tu/prop! (editable-node-ids-by-key node-key) :__atlas (tu/resource workspace atlas-proj-path)))
                (let [editable-results
                      {:built-source-paths {:chair (tu/node-built-source-paths chair)
                                            :room (tu/node-built-source-paths room)
                                            :house (tu/node-built-source-paths house)}
                       :build-targets {:chair (g/node-value chair :build-targets)
                                       :room (g/node-value room :build-targets)
                                       :house (g/node-value house :build-targets)}}]
                  (save-project! project)
                  (set-non-editable-directories! project-path ["/assets"])
                  editable-results)))]
        ;; Reload the project now that the resources are in a non-editable state
        ;; and compare the non-editable output to the editable output.
        (with-clean-system
          (let [non-editable-node-ids-by-key (load-project! world project-path)]

            (testing "Ensure the resources have been loaded in a non-editable state."
              (is (not-any? #(some-> % val resource-node/as-resource-original resource/editable?)
                            non-editable-node-ids-by-key)))

            (testing "Verify the built source paths match."
              (doseq [[node-key editable-build-source-paths] (:built-source-paths editable-results)
                      :let [non-editable-node-id (non-editable-node-ids-by-key node-key)]]
                (testing (str node-key)
                  (is (= editable-build-source-paths
                         (tu/node-built-source-paths non-editable-node-id))))))

            (testing "Verify the contents of the build targets match."
              (doseq [[node-key editable-build-targets] (:build-targets editable-results)
                      :let [non-editable-node-id (non-editable-node-ids-by-key node-key)]]
                (testing (str node-key)
                  (let [non-editable-build-targets (g/node-value non-editable-node-id :build-targets)]
                    (when (and (is (not (g/error? editable-build-targets)))
                               (is (not (g/error? non-editable-build-targets))))
                      (is (= (comparable-build-targets editable-build-targets)
                             (comparable-build-targets non-editable-build-targets))))))))))))))

(deftest non-editable-resources-test
  (doseq [atlas-property-proj-paths-by-node-key
          [{}

           {:chair-script "/assets/from-chair-script.atlas"}

           {:room-referenced-chair-script "/assets/from-room-referenced-chair-script.atlas"}

           {:house-room-embedded-chair-script "/assets/from-house-room-embedded-chair-script.atlas"}

           {:house-room-referenced-chair-script "/assets/from-house-room-referenced-chair-script.atlas"}

           {:chair-script "/assets/from-chair-script.atlas"
            :room-referenced-chair-script "/assets/from-room-referenced-chair-script.atlas"}

           {:chair-script "/assets/from-chair-script.atlas"
            :house-room-embedded-chair-script "/assets/from-house-room-embedded-chair-script.atlas"}

           {:chair-script "/assets/from-chair-script.atlas"
            :house-room-referenced-chair-script "/assets/from-house-room-referenced-chair-script.atlas"}

           {:room-referenced-chair-script "/assets/from-room-referenced-chair-script.atlas"
            :house-room-embedded-chair-script "/assets/from-house-room-embedded-chair-script.atlas"}

           {:room-referenced-chair-script "/assets/from-room-referenced-chair-script.atlas"
            :house-room-referenced-chair-script "/assets/from-house-room-referenced-chair-script.atlas"}

           {:chair-script "/assets/from-chair-script.atlas"
            :room-referenced-chair-script "/assets/from-room-referenced-chair-script.atlas"
            :house-room-embedded-chair-script "/assets/from-house-room-embedded-chair-script.atlas"}

           {:chair-script "/assets/from-chair-script.atlas"
            :room-referenced-chair-script "/assets/from-room-referenced-chair-script.atlas"
            :house-room-referenced-chair-script "/assets/from-house-room-referenced-chair-script.atlas"}]]
    (perform-non-editable-resources-test atlas-property-proj-paths-by-node-key)))

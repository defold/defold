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

(ns integration.non-editable-resources-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.fs :as fs]
            [editor.game-project :as game-project]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.workspace :as workspace]
            [integration.test-util :as tu]
            [internal.util :as util]
            [service.log :as log]
            [support.test-support :refer [with-clean-system]]
            [util.coll :refer [pair]]
            [util.digestable :as digestable])
  (:import [com.dynamo.bob.textureset TextureSetGenerator$UVTransform]
           [com.dynamo.gameobject.proto GameObject$CollectionDesc]
           [com.google.protobuf ByteString]
           [com.jogamp.opengl.util.texture TextureData]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.texture TextureLifecycle]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; Helpers
;; -----------------------------------------------------------------------------

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

(defn comparable-record
  ([record value-fn]
   (comparable-record record value-fn #{}))
  ([record value-fn excluded-keys]
   (let [exclude-key? (set excluded-keys)]
     (into (sorted-map :- (class-symbol record))
           (keep (fn [[k v]]
                   (when-not (exclude-key? k)
                     (let [v' (util/deep-map-kv-helper identity value-fn k v)]
                       (pair k v')))))
           record))))

(defn comparable-vecmath [value]
  (mapv #(math/round-with-precision % 0.0001)
        (math/vecmath->clj value)))

(def ^:private comparable-excluded-key?
  #{:node-outline-key
    :picking-node-id
    :select-batch-key})

(defn- comparable-output [output]
  (letfn [(value-fn [key value]
            (cond
              (comparable-excluded-key? key)
              nil ; Exclude the entry.

              (nil? value)
              :nil

              (digestable/node-id-entry? key value)
              :node-id ; Node ids won't ever match.

              (instance? ByteString value)
              {:ByteString (into (vector-of :byte) value)}

              (or (instance? ShaderLifecycle value) (instance? TextureLifecycle value))
              (comparable-record value value-fn #{:request-id}) ; The :request-id typically involves the node-id.

              (instance? TextureData value)
              {:TextureData (digestable/sha1-hash (str value))}

              (instance? TextureSetGenerator$UVTransform value)
              (let [^TextureSetGenerator$UVTransform uv-transform value]
                {:- (class-symbol uv-transform)
                 :rotated (.-rotated uv-transform)
                 :scale (comparable-vecmath (.-scale uv-transform))
                 :translation (comparable-vecmath (.-translation uv-transform))})

              (resource/resource? value)
              (merge (sorted-map (class-keyword value) (resource/proj-path value))
                     (extra-comparable-resource-data value))

              (satisfies? math/VecmathConverter value)
              {(class-keyword value) (comparable-vecmath value)}

              (record? value)
              (comparable-record value value-fn)

              (fn? value)
              {:Fn (digestable/fn->symbol value)}

              :else
              value))]
    (util/deep-keep-kv util/with-sorted-keys value-fn output)))

(defn- load-non-editable-project! [world project-path proj-paths-by-node-key]
  (let [workspace (log/without-logging (tu/setup-workspace! world project-path))
        project (tu/setup-project! workspace)

        non-editable-node-ids-by-node-key
        (into {:project project
               :workspace workspace}
              (map (fn [[node-key proj-path]]
                     (pair node-key
                           (tu/resource-node project proj-path))))
              proj-paths-by-node-key)]

    (testing "Ensure the resources have been loaded in a non-editable state."
      (is (not-any? #(some-> % val resource-node/as-resource-original resource/editable?)
                    non-editable-node-ids-by-node-key)))

    non-editable-node-ids-by-node-key))

(defn- compare-output [output-label node-key+editable-output-pairs-by-output-label non-editable-node-ids-by-node-key]
  (testing (str "Verify the produced outputs match for " output-label ".")
    (let [node-key+editable-output-pairs (node-key+editable-output-pairs-by-output-label output-label)]
      (is (seq node-key+editable-output-pairs))
      (doseq [[node-key editable-output] node-key+editable-output-pairs
              :let [non-editable-node-id (non-editable-node-ids-by-node-key node-key)]]
        (testing (str node-key)
          (let [non-editable-output (g/node-value non-editable-node-id output-label)]
            (when (and (is (not (g/error? editable-output)))
                       (is (not (g/error? non-editable-output))))
              (is (= (comparable-output editable-output)
                     (comparable-output non-editable-output))))))))))

;; -----------------------------------------------------------------------------
;; build-target-test
;; -----------------------------------------------------------------------------

(defn- create-build-target-test-project! [world project-path atlas-proj-paths]
  (let [workspace (tu/setup-workspace! world project-path)
        project (tu/setup-project! workspace)]
    (tu/make-atlas-resource-node! project "/assets/from-script.atlas")
    (tu/make-atlas-resource-node! project "/assets/from-chair-embedded-sprite.atlas")
    (tu/make-atlas-resource-node! project "/assets/from-room-embedded-chair-embedded-sprite.atlas")
    (doseq [atlas-proj-path atlas-proj-paths]
      (tu/make-atlas-resource-node! project atlas-proj-path))
    (let [sprite-resource-type (workspace/get-resource-type workspace "sprite")
          script (doto (tu/make-resource-node! project "/assets/script.script")
                   (tu/set-code-editor-source! "go.property('atlas', resource.atlas('/assets/from-script.atlas'))"))
          chair (tu/make-resource-node! project "/assets/chair.go")
          chair-referenced-script (tu/add-referenced-component! chair (resource-node/resource script))
          chair-embedded-sprite (tu/add-embedded-component! chair sprite-resource-type)
          room (tu/make-resource-node! project "/assets/room.collection")
          room-embedded-chair (tu/add-embedded-game-object! room)
          room-embedded-chair-referenced-script (tu/add-referenced-component! room-embedded-chair (resource-node/resource script))
          room-embedded-chair-embedded-sprite (tu/add-embedded-component! room-embedded-chair sprite-resource-type)
          room-referenced-chair (tu/add-referenced-game-object! room (resource-node/resource chair))
          room-referenced-chair-referenced-script (tu/referenced-component room-referenced-chair)
          house (tu/make-resource-node! project "/assets/house.collection")
          house-room (tu/add-referenced-collection! house (resource-node/resource room))
          house-room-embedded-chair (tu/embedded-game-object house-room)
          house-room-embedded-chair-referenced-script (tu/referenced-component house-room-embedded-chair)
          house-room-referenced-chair (tu/referenced-game-object house-room)
          house-room-referenced-chair-referenced-script (tu/referenced-component house-room-referenced-chair)]
      (g/transact
        (concat
          (g/set-property chair-referenced-script :id "chair-referenced-script")
          (g/set-property chair-embedded-sprite :id "chair-embedded-sprite")
          (g/set-property room-embedded-chair :id "room-embedded-chair")
          (g/set-property room-embedded-chair-referenced-script :id "room-embedded-chair-referenced-script")
          (g/set-property room-embedded-chair-embedded-sprite :id "room-embedded-chair-embedded-sprite")
          (g/set-property room-referenced-chair :id "room-referenced-chair")
          (g/set-property house-room :id "house-room")))
      (tu/prop! chair-embedded-sprite :__sampler__texture_sampler__0 (tu/resource workspace "/assets/from-chair-embedded-sprite.atlas"))
      (tu/prop! chair-embedded-sprite :default-animation "from-chair-embedded-sprite")
      (tu/prop! room-embedded-chair-embedded-sprite :__sampler__texture_sampler__0 (tu/resource workspace "/assets/from-room-embedded-chair-embedded-sprite.atlas"))
      (tu/prop! room-embedded-chair-embedded-sprite :default-animation "from-room-embedded-chair-embedded-sprite")
      {:chair chair
       :chair-referenced-script chair-referenced-script
       :house house
       :house-room-embedded-chair-referenced-script house-room-embedded-chair-referenced-script
       :house-room-referenced-chair-referenced-script house-room-referenced-chair-referenced-script
       :project project
       :room room
       :room-embedded-chair-referenced-script room-embedded-chair-referenced-script
       :room-referenced-chair-referenced-script room-referenced-chair-referenced-script
       :script script
       :workspace workspace})))

(defn- load-build-target-test-project! [world project-path]
  (load-non-editable-project!
    world project-path
    {:chair "/assets/chair.go"
     :house "/assets/house.collection"
     :room "/assets/room.collection"}))

(defn- perform-build-target-test [atlas-property-proj-paths-by-node-key]
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
                            workspace] :as editable-node-ids-by-node-key} (create-build-target-test-project! world project-path atlas-proj-paths)]
                (doseq [[node-key atlas-proj-path] atlas-property-proj-paths-by-node-key]
                  (tu/prop! (editable-node-ids-by-node-key node-key) :__atlas (tu/resource workspace atlas-proj-path)))
                (let [editable-results
                      {:built-source-paths {:chair (tu/node-built-source-paths chair)
                                            :room (tu/node-built-source-paths room)
                                            :house (tu/node-built-source-paths house)}
                       :build-targets {:chair (g/node-value chair :build-targets)
                                       :room (g/node-value room :build-targets)
                                       :house (g/node-value house :build-targets)}}]
                  (tu/save-project! project)
                  (tu/set-non-editable-directories! project-path ["/assets"])
                  editable-results)))]
        ;; Reload the project now that the resources are in a non-editable state
        ;; and compare the non-editable output to the editable output.
        (with-clean-system
          (let [non-editable-node-ids-by-node-key (load-build-target-test-project! world project-path)]
            (compare-output :build-targets editable-results non-editable-node-ids-by-node-key)

            (testing "Verify the built source paths match."
              (doseq [[node-key editable-build-source-paths] (:built-source-paths editable-results)
                      :let [non-editable-node-id (non-editable-node-ids-by-node-key node-key)]]
                (testing (str node-key)
                  (is (= editable-build-source-paths
                         (tu/node-built-source-paths non-editable-node-id))))))))))))

(deftest build-target-test
  (doseq [atlas-property-proj-paths-by-node-key
          [{}

           {:chair-referenced-script "/assets/from-chair-referenced-script.atlas"}

           {:room-referenced-chair-referenced-script "/assets/from-room-referenced-chair-referenced-script.atlas"}

           {:house-room-embedded-chair-referenced-script "/assets/from-house-room-embedded-chair-referenced-script.atlas"}

           {:house-room-referenced-chair-referenced-script "/assets/from-house-room-referenced-chair-referenced-script.atlas"}

           {:chair-referenced-script "/assets/from-chair-referenced-script.atlas"
            :room-referenced-chair-referenced-script "/assets/from-room-referenced-chair-referenced-script.atlas"}

           {:chair-referenced-script "/assets/from-chair-referenced-script.atlas"
            :house-room-embedded-chair-referenced-script "/assets/from-house-room-embedded-chair-referenced-script.atlas"}

           {:chair-referenced-script "/assets/from-chair-referenced-script.atlas"
            :house-room-referenced-chair-referenced-script "/assets/from-house-room-referenced-chair-referenced-script.atlas"}

           {:room-referenced-chair-referenced-script "/assets/from-room-referenced-chair-referenced-script.atlas"
            :house-room-embedded-chair-referenced-script "/assets/from-house-room-embedded-chair-referenced-script.atlas"}

           {:room-referenced-chair-referenced-script "/assets/from-room-referenced-chair-referenced-script.atlas"
            :house-room-referenced-chair-referenced-script "/assets/from-house-room-referenced-chair-referenced-script.atlas"}

           {:chair-referenced-script "/assets/from-chair-referenced-script.atlas"
            :room-referenced-chair-referenced-script "/assets/from-room-referenced-chair-referenced-script.atlas"
            :house-room-embedded-chair-referenced-script "/assets/from-house-room-embedded-chair-referenced-script.atlas"}

           {:chair-referenced-script "/assets/from-chair-referenced-script.atlas"
            :room-referenced-chair-referenced-script "/assets/from-room-referenced-chair-referenced-script.atlas"
            :house-room-referenced-chair-referenced-script "/assets/from-house-room-referenced-chair-referenced-script.atlas"}]]
    (perform-build-target-test atlas-property-proj-paths-by-node-key)))

(deftest build-target-fusion-test
  (let [project-path (tu/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (tu/make-directory-deleter project-path)]
      (tu/set-non-editable-directories! project-path ["/non-editable"])
      (with-clean-system
        ;; Create a collection with an embedded game object with an embedded
        ;; component. Then, make a copy of the collection resource under a
        ;; non-editable directory, and reference both collections from the main
        ;; collection.  We want to ensure the build targets for the embedded
        ;; resources are fused into one.
        (let [workspace (tu/setup-workspace! world project-path)
              project (tu/setup-project! workspace)
              game-project (tu/resource-node project "/game.project")
              main-collection (tu/make-resource-node! project "/main.collection")]
          (game-project/set-setting! game-project ["bootstrap" "main_collection"] (resource-node/resource main-collection))
          (let [editable-room (tu/make-resource-node! project "/editable/room.collection")]
            (is (resource/editable? (resource-node/resource editable-room)))
            (is (= :editor.collection/CollectionNode (g/node-type-kw editable-room)))
            (-> editable-room
                (tu/add-embedded-game-object!)
                (tu/add-embedded-component! (workspace/get-resource-type workspace "camera")))
            (-> (tu/add-referenced-collection! main-collection (resource-node/resource editable-room))
                (tu/prop! :id "editable-room")))
          (tu/save-project! project)
          (fs/copy-directory! (io/file project-path "editable")
                              (io/file project-path "non-editable"))
          (workspace/resource-sync! workspace)
          (let [non-editable-room (tu/resource-node project "/non-editable/room.collection")]
            (is (not (resource/editable? (resource-node/resource non-editable-room))))
            (is (= :editor.collection-non-editable/NonEditableCollectionNode (g/node-type-kw non-editable-room)))
            (-> (tu/add-referenced-collection! main-collection (resource-node/resource non-editable-room))
                (tu/prop! :id "non-editable-room")))

          (testing "Build targets from embedded resources inside editable resources fuse with equivalents from non-editable resources."
            (with-open [_ (tu/build! main-collection)]
              (let [main-collection-pb-map (protobuf/bytes->map-without-defaults GameObject$CollectionDesc (tu/node-build-output main-collection))
                    [editable-room-instance-pb-map non-editable-room-instance-pb-map] (:instances main-collection-pb-map)]
                (is (= "/editable-room/go" (:id editable-room-instance-pb-map)))
                (is (= "/non-editable-room/go" (:id non-editable-room-instance-pb-map)))
                (is (string/includes? (:prototype editable-room-instance-pb-map) "generated"))
                (is (= (:prototype editable-room-instance-pb-map)
                       (:prototype non-editable-room-instance-pb-map)))))))))))

;; -----------------------------------------------------------------------------
;; scene-test
;; -----------------------------------------------------------------------------

(defn- create-scene-test-project! [world project-path]
  (let [workspace (tu/setup-workspace! world project-path)
        project (tu/setup-project! workspace)]
    (tu/make-atlas-resource-node! project "/assets/from-sprite.atlas")
    (tu/make-atlas-resource-node! project "/assets/from-chair-embedded-sprite.atlas")
    (tu/make-atlas-resource-node! project "/assets/from-room-embedded-chair-embedded-sprite.atlas")
    (let [sprite-resource-type (workspace/get-resource-type workspace "sprite")
          script (tu/make-resource-node! project "/assets/script.script")
          sprite (tu/make-resource-node! project "/assets/sprite.sprite")
          chair (tu/make-resource-node! project "/assets/chair.go")
          chair-embedded-sprite (tu/add-embedded-component! chair sprite-resource-type)
          chair-referenced-script (tu/add-referenced-component! chair (resource-node/resource script))
          chair-referenced-sprite (tu/add-referenced-component! chair (resource-node/resource sprite))
          room (tu/make-resource-node! project "/assets/room.collection")
          room-embedded-chair (tu/add-embedded-game-object! room)
          room-embedded-chair-embedded-sprite (tu/add-embedded-component! room-embedded-chair sprite-resource-type)
          room-embedded-chair-referenced-script (tu/add-referenced-component! room-embedded-chair (resource-node/resource script))
          room-embedded-chair-referenced-sprite (tu/add-referenced-component! room-embedded-chair (resource-node/resource sprite))
          room-referenced-chair (tu/add-referenced-game-object! room (resource-node/resource chair))
          house (tu/make-resource-node! project "/assets/house.collection")
          house-referenced-room (tu/add-referenced-collection! house (resource-node/resource room))]

      (doto sprite
        (tu/prop! :__sampler__texture_sampler__0 (tu/resource workspace "/assets/from-sprite.atlas"))
        (tu/prop! :default-animation "from-sprite"))

      (doto chair-embedded-sprite
        (tu/prop! :id "chair-embedded-sprite")
        (tu/prop! :position [1.1 1.2 1.3])
        (tu/prop! :rotation (math/vecmath->clj (math/euler-z->quat 1.0)))
        (tu/prop! :scale [1.4 1.5 1.6])
        (tu/prop! :__sampler__texture_sampler__0 (tu/resource workspace "/assets/from-chair-embedded-sprite.atlas"))
        (tu/prop! :default-animation "from-chair-embedded-sprite"))

      (doto chair-referenced-script
        (tu/prop! :id "chair-referenced-script"))

      (doto chair-referenced-sprite
        (tu/prop! :id "chair-referenced-sprite")
        (tu/prop! :position [2.1 2.2 2.3])
        (tu/prop! :rotation (math/vecmath->clj (math/euler-z->quat 2.0)))
        (tu/prop! :scale [2.4 2.5 2.6]))

      (doto room-embedded-chair
        (tu/prop! :id "room-embedded-chair")
        (tu/prop! :position [3.1 3.2 3.3])
        (tu/prop! :rotation (math/vecmath->clj (math/euler-z->quat 3.0)))
        (tu/prop! :scale [3.4 3.5 3.6]))

      (doto room-embedded-chair-embedded-sprite
        (tu/prop! :id "room-embedded-chair-embedded-sprite")
        (tu/prop! :position [4.1 4.2 4.3])
        (tu/prop! :rotation (math/vecmath->clj (math/euler-z->quat 4.0)))
        (tu/prop! :scale [4.4 4.5 4.6])
        (tu/prop! :__sampler__texture_sampler__0 (tu/resource workspace "/assets/from-room-embedded-chair-embedded-sprite.atlas"))
        (tu/prop! :default-animation "from-room-embedded-chair-embedded-sprite"))

      (doto room-embedded-chair-referenced-script
        (tu/prop! :id "room-embedded-chair-referenced-script"))

      (doto room-embedded-chair-referenced-sprite
        (tu/prop! :id "room-embedded-chair-referenced-sprite")
        (tu/prop! :position [5.1 5.2 5.3])
        (tu/prop! :rotation (math/vecmath->clj (math/euler-z->quat 5.0)))
        (tu/prop! :scale [5.4 5.5 5.6]))

      (doto room-referenced-chair
        (tu/prop! :id "room-referenced-chair")
        (tu/prop! :position [6.1 6.2 6.3])
        (tu/prop! :rotation (math/vecmath->clj (math/euler-z->quat 6.0)))
        (tu/prop! :scale [6.4 6.5 6.6]))

      (doto house-referenced-room
        (tu/prop! :id "house-referenced-room")
        (tu/prop! :position [7.1 7.2 7.3])
        (tu/prop! :rotation (math/vecmath->clj (math/euler-z->quat 7.0)))
        (tu/prop! :scale [7.4 7.5 7.6]))

      {:chair chair
       :house house
       :project project
       :room room
       :workspace workspace})))

(defn- load-scene-test-project! [world project-path]
  (load-non-editable-project!
    world project-path
    {:chair "/assets/chair.go"
     :house "/assets/house.collection"
     :room "/assets/room.collection"}))

(deftest scene-test
  (let [project-path (tu/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (tu/make-directory-deleter project-path)]
      ;; Populate the project and capture output from the resource nodes while
      ;; they are in an editable state. Then mark the resources as non-editable.
      (let [editable-results
            (with-clean-system
              (let [{:keys [chair
                            house
                            project
                            room] :as _editable-node-ids-by-node-key} (create-scene-test-project! world project-path)]
                (let [editable-results
                      {:scene {:chair (g/node-value chair :scene)
                               :room (g/node-value room :scene)
                               :house (g/node-value house :scene)}}]
                  (tu/save-project! project)
                  (tu/set-non-editable-directories! project-path ["/assets"])
                  editable-results)))]
        ;; Reload the project now that the resources are in a non-editable state
        ;; and compare the non-editable output to the editable output.
        (with-clean-system
          (let [non-editable-node-ids-by-node-key (load-scene-test-project! world project-path)]
            (compare-output :scene editable-results non-editable-node-ids-by-node-key)))))))

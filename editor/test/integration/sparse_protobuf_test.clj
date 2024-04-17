;; Copyright 2020-2024 The Defold Foundation
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

(ns integration.sparse-protobuf-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.util :as util]
            [support.test-support :as test-support]
            [util.coll :refer [pair]])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [com.dynamo.gamesys.proto ModelProto$ModelDesc]
           [java.io File]))

(set! *warn-on-reflection* true)

;; This is the limit to how deeply we iterate into nested protobuf messages.
;; Ten is plenty. Currently, our deepest file formats only reach depth04.
(def ^:private ^:const max-pb-map-depth 10)

;; Some file formats require real-world protobuf field values.
;; This map controls what values get injected into the generated files.
(def ^:private pb-field-values
  (let [tile-source
        {:image "/builtins/graphics/particle_blob.png"
         :tile-width 8
         :tile-height 8
         :animations [{:id "default"
                       :start-tile 1
                       :end-tile 1}]}

        vertex-attribute-override
        {:name "orphaned"
         :double-values {:v [0.0]}}]

    {"cubemap" {:back "/builtins/graphics/particle_blob.png"
                :bottom "/builtins/graphics/particle_blob.png"
                :front "/builtins/graphics/particle_blob.png"
                :left "/builtins/graphics/particle_blob.png"
                :right "/builtins/graphics/particle_blob.png"
                :top "/builtins/graphics/particle_blob.png"}
     "font" {:font "/builtins/fonts/vera_mo_bd.ttf"
             :material "/builtins/fonts/font.material"
             :size 10}
     "go" {:embedded-components [{:type "factory"
                                  :data "prototype: ''"}]}
     "label" {:font "/builtins/fonts/default.font"
              :material "/builtins/fonts/label-df.material"}
     "model" {:materials [{:attributes [vertex-attribute-override]}]}
     "sprite" {:attributes [vertex-attribute-override]}
     "tileset" tile-source
     "tilesource" tile-source}))

(defn- sanctioned-extension-uris []
  (into (sorted-set)
        (keep (fn [[key value]]
                (when (re-matches #"^defold\.extension\..+\.url$" key)
                  value)))
        (System/getProperties)))

(defn- resource-type->pb-class
  ^Class [resource-type]
  (-> resource-type :test-info :ddf-type))

(defn- relevant-protobuf-resource-type? [resource-type]
  (and (ifn? (:load-fn resource-type))
       (class? (resource-type->pb-class resource-type))))

(defn- sparse-pb-map [^Class pb-class pb-path ^long depth-limit]
  (into {}
        (keep (fn [[field-key {:keys [default field-rule field-type-key options type]}]]
                (let [pb-path (conj pb-path field-key)
                      value (case field-rule
                              :required
                              (if-some [specific-value (get-in pb-field-values pb-path)]
                                specific-value
                                (if (= :message field-type-key)
                                  (sparse-pb-map type pb-path (dec depth-limit))
                                  default))

                              :optional
                              (if-some [specific-value (get-in pb-field-values pb-path)]
                                specific-value
                                (when (and (= :message field-type-key)
                                           (pos? depth-limit)
                                           (not (:runtime-only options)))
                                  (sparse-pb-map type pb-path (dec depth-limit))))

                              :repeated
                              (let [pb-path (conj pb-path 0)]
                                (when (and (= :message field-type-key)
                                           (pos? depth-limit)
                                           (not (:runtime-only options)))
                                  (some-> (sparse-pb-map type pb-path (dec depth-limit))
                                          (vector)))))]
                  (when (some? value)
                    (pair field-key value)))))
        (protobuf/field-infos pb-class)))

(defn- protobuf-resource-types-by-editability [workspace]
  (let [editable-protobuf-resource-types
        (into (sorted-map)
              (filter (fn [[_ext editable-resource-type]]
                        (relevant-protobuf-resource-type? editable-resource-type)))
              (workspace/get-resource-type-map workspace :editable))

        distinctly-non-editable-protobuf-resource-types
        (into (sorted-map)
              (filter (fn [[ext non-editable-resource-type]]
                        (and (relevant-protobuf-resource-type? non-editable-resource-type)
                             (not (identical? non-editable-resource-type
                                              (editable-protobuf-resource-types ext))))))
              (workspace/get-resource-type-map workspace :non-editable))]

    {:editable (into [] (map val) editable-protobuf-resource-types)
     :non-editable (into [] (map val) distinctly-non-editable-protobuf-resource-types)}))

(defn- sparse-protobuf-content-by-proj-path [workspace]
  (into (sorted-map)
        (mapcat (fn [[editability resource-types]]
                  (eduction
                    (mapcat (fn [resource-type]
                              (let [ext (:ext resource-type)
                                    pb-class (resource-type->pb-class resource-type)
                                    pb-path [ext]]
                                (eduction
                                  (map (fn [^long depth-limit]
                                         (let [pb-map (sparse-pb-map pb-class pb-path depth-limit)
                                               content (protobuf/map->str pb-class pb-map)]
                                           (pair depth-limit content))))
                                  (util/distinct-by val)
                                  (map (fn [[^long depth-limit content]]
                                         (let [proj-path (format "/%s/depth%02d.%s" (name editability) depth-limit ext)]
                                           (pair proj-path content))))
                                  (range max-pb-map-depth)))))
                    resource-types)))
        (protobuf-resource-types-by-editability workspace)))

(defn- write-sparse-protobuf-resources! [^File project-root-directory sparse-protobuf-content-by-proj-path]
  (let [sparse-protobuf-content-by-absolute-file
        (into (sorted-map)
              (map (fn [[proj-path content]]
                     (let [relative-path (subs proj-path 1) ; Strip leading slash.
                           absolute-file (io/file project-root-directory relative-path)]
                       (pair absolute-file content))))
              sparse-protobuf-content-by-proj-path)

        absolute-directories
        (into (sorted-set)
              (keep (fn [[^File absolute-file]]
                      (.getParentFile absolute-file)))
              sparse-protobuf-content-by-absolute-file)]

    ;; Create parent directories in the project.
    (doseq [directory absolute-directories]
      (fs/create-directories! directory))

    ;; Write files in the project.
    (doseq [[absolute-file content] sparse-protobuf-content-by-absolute-file]
      (test-support/spit-until-new-mtime absolute-file content))))

(deftest sparse-pb-map-meta-test
  (testing "go"
    (is (= {}
           (sparse-pb-map GameObject$PrototypeDesc ["go"] 0)))
    (is (= {:components [{:id ""
                          :component ""}]
            :embedded-components [{:id ""
                                   :type "factory"
                                   :data "prototype: ''"}]}
           (sparse-pb-map GameObject$PrototypeDesc ["go"] 1)))
    (is (= {:components [{:id ""
                          :component ""
                          :position {}
                          :rotation {}
                          :scale {}
                          :properties [{:id ""
                                        :type :property-type-number
                                        :value ""}]}]
            :embedded-components [{:id ""
                                   :type "factory"
                                   :data "prototype: ''"
                                   :position {}
                                   :rotation {}
                                   :scale {}}]}
           (sparse-pb-map GameObject$PrototypeDesc ["go"] 2))))

  (testing "model"
    (is (= {:mesh ""}
           (sparse-pb-map ModelProto$ModelDesc ["model"] 0)))
    (is (= {:mesh ""
            :materials [{:name ""
                         :material ""}]}
           (sparse-pb-map ModelProto$ModelDesc ["model"] 1)))
    (is (= {:mesh ""
            :materials [{:name ""
                         :material ""
                         :textures [{:sampler ""
                                     :texture ""}]
                         :attributes [{:name "orphaned"
                                       :double-values {:v [0.0]}}]}]}
           (sparse-pb-map ModelProto$ModelDesc ["model"] 2)))))

(deftest sparse-protobuf-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path [(str "/" (name :non-editable))])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          ;; TODO(save-value): Add dependencies to all sanctioned extensions to game.project.
          #_ (let [sanctioned-extension-uris (sanctioned-extension-uris)]
               (test-util/set-libraries! workspace sanctioned-extension-uris))

          ;; With the extensions added, we can populate the workspace with
          ;; sparse files for all the protobuf resource types.
          (let [sparse-protobuf-content-by-proj-path (sparse-protobuf-content-by-proj-path workspace)
                project-root-directory (workspace/project-path workspace)]
            (write-sparse-protobuf-resources! project-root-directory sparse-protobuf-content-by-proj-path)
            (workspace/resource-sync! workspace)

            ;; With the workspace populated, we can load the project.
            (let [project (test-util/setup-project! workspace)
                  proj-paths (keys sparse-protobuf-content-by-proj-path)]
              (doseq [proj-path proj-paths]
                (let [resource (workspace/find-resource workspace proj-path)
                      resource-node (project/get-resource-node project resource)
                      resource-type (resource/resource-type resource)
                      openable-in-view-type? (into #{} (map :id) (:view-types resource-type))]
                  (testing proj-path
                    (is (not (g/error? (g/node-value resource-node :save-data))))
                    (is (not (g/error? (g/node-value resource-node :_properties))))
                    (when (openable-in-view-type? :cljfx-form-view)
                      (is (not (g/error? (g/node-value resource-node :form-data)))))
                    (when (openable-in-view-type? :scene)
                      (is (not (g/error? (g/node-value resource-node :scene)))))))))))))))

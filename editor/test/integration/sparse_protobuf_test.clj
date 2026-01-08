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

(ns integration.sparse-protobuf-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.util :as util]
            [support.test-support :as test-support]
            [util.coll :as coll :refer [pair]])
  (:import [clojure.lang IHashEq ILookup Util]
           [com.dynamo.gameobject.proto GameObject$CollectionDesc GameObject$PrototypeDesc]
           [com.dynamo.gamesys.proto GameSystem$FactoryDesc Gui$NodeDesc ModelProto$ModelDesc Physics$CollisionObjectDesc]
           [java.io File Writer]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private supplemental-save-values-by-proj-path
  {"/referenced/empty.particlefx"
   {}

   "/referenced/simple.atlas"
   {:images [{:image "/builtins/graphics/particle_blob.png"}]
    :animations [{:id "animation_id"
                  :images [{:image "/builtins/graphics/particle_blob.png"}]}]}

   "/referenced/instanced.collection"
   {:name "instanced_collection"
    :instances [{:id "instance_id"
                 :prototype "/referenced/instanced.go"}]}

   "/referenced/instanced.go"
   {:components [{:id "component_id"
                  :component "/referenced/prop.script"}]}

   "/referenced/prop.script"
   ["go.property('property_id', 0.0)"]})

(deftype ^:private Required [value]
  IHashEq
  (hasheq [_]
    (Util/hasheq value))
  ILookup
  (valAt [_ key]
    (get value key))
  (valAt [_ key not-found]
    (get value key not-found))
  Object
  (toString [_]
    (str "(required " (pr-str value) ")"))
  (hashCode [_]
    (Util/hash value))
  (equals [_ that]
    (Util/equals value (.-value ^Required that))))

(defmethod print-method Required [^Required required, ^Writer writer]
  (.write writer "(required ")
  (print-method (.-value required) writer)
  (.write writer ")"))

(defn- required? [value]
  (instance? Required value))

(defn- required [value]
  (cond
    (required? value)
    (required (.-value ^Required value))

    (record? value)
    value

    (map? value)
    (Required.
      (into (coll/empty-with-meta value)
            (map (fn [[k v]]
                   (pair k (required v))))
            value))

    (coll? value)
    (Required.
      (into (coll/empty-with-meta value)
            (map required)
            value))

    :else
    (Required. value)))

(defn- unwrap-if-required [value]
  (cond
    (required? value)
    (unwrap-if-required (.-value ^Required value))

    (record? value)
    value

    (map? value)
    (into (coll/empty-with-meta value)
          (map (fn [[k v]]
                 (pair k (unwrap-if-required v))))
          value)

    (coll? value)
    (into (coll/empty-with-meta value)
          (map unwrap-if-required)
          value)

    :else
    value))

(defn- unwrap-pb-field-value [pb-field-value]
  (let [value (unwrap-if-required pb-field-value)
        is-required (required? pb-field-value)]
    (pair value is-required)))

(defrecord ^:private Exactly [value])

(defmethod print-method Exactly [^Exactly exactly, ^Writer writer]
  (.write writer "(exactly ")
  (print-method (.-value exactly) writer)
  (.write writer ")"))

(defn- exactly? [value]
  (instance? Exactly value))

(defn- unwrap-exactly [^Exactly value]
  (.-value value))

(defn- exactly [value]
  (assert (not (exactly? value)))
  (Exactly. (unwrap-if-required value)))

;; Some file formats require real-world protobuf field values.
;; This map controls what values get injected into the generated files.
(def ^:private pb-field-values
  (let [image-proj-path "/builtins/graphics/particle_blob.png"

        property-desc
        {:id "property_id"
         :type :property-type-number
         :value "1.0"}

        component-property-desc
        {:id "component_id"
         :properties (required property-desc)}

        tile-source
        {:image image-proj-path
         :tile-width 8
         :tile-height 8
         :convex-hulls (exactly nil) ; TODO(save-value-cleanup): Weird semantics. Skip testing convex hulls for now.
         :animations {:id "animation_id"
                      :start-tile 1
                      :end-tile 1}}

        vertex-attribute
        {:name "attribute_name"
         :double-values (required {:v 0.0})}

        embedded-component-data
        (protobuf/map->str
          GameSystem$FactoryDesc
          {:prototype "/referenced/instanced.go"})

        embedded-instance-data
        (protobuf/map->str
          GameObject$PrototypeDesc
          {:components [{:id "component_id"
                         :component "/referenced/prop.script"
                         :properties [property-desc]}]
           :embedded-components [{:id "embedded_component_id"
                                  :type "factory"
                                  :data embedded-component-data}]})]

    {"atlas"
     {:images {:image image-proj-path}
      :animations {:id "animation_id"
                   :images {:image image-proj-path}}}

     "collection"
     {:collection-instances {:id "collection_instance_id"
                             :collection "/referenced/instanced.collection"
                             :instance-properties {:id "instance_id"
                                                   :properties (required component-property-desc)}}
      :embedded-instances {:id "instance_id"
                           :data embedded-instance-data
                           :component-properties (exactly nil)}
      :instances {:id "instance_id"
                  :prototype "/referenced/instanced.go"
                  :component-properties component-property-desc}}

     "collisionobject"
     {:embedded-collision-shape {:shapes {:count 1}
                                 :data 1.0}}

     "cubemap"
     {:back image-proj-path
      :bottom image-proj-path
      :front image-proj-path
      :left image-proj-path
      :right image-proj-path
      :top image-proj-path}

     "font"
     {:font "/builtins/fonts/vera_mo_bd.ttf"
      :material "/builtins/fonts/font.material"
      :size 10}

     "go"
     {:components {:id "component_id"
                   :component "/referenced/prop.script"
                   :properties property-desc}
      :embedded-components {:id "embedded_component_id"
                            :type "factory"
                            :data embedded-component-data}}

     "gui"
     {:nodes {:id (required "node_id")}
      :layouts {:name "layout_name"
                :nodes {:id (required "node_id")
                        :alpha (required 0.5)
                        :overridden-fields (required Gui$NodeDesc/ALPHA_FIELD_NUMBER)}}
      :layers {:name "layer_name"}
      :fonts {:name "font_name"
              :font "/builtins/fonts/default.font"}
      :materials {:name "material_name"
                  :material "/builtins/materials/gui.material"}
      :particlefxs {:name "particlefx_name"
                    :particlefx "/referenced/empty.particlefx"}
      :resources {:name "resource_name"
                  :path "/defold-spine/assets/template/template.spinescene"}
      :spine-scenes {:name "spine_scene_name"
                     :spine-scene "/defold-spine/assets/template/template.spinescene"}
      :textures {:name "texture_name"
                 :texture "/referenced/simple.atlas"}}

     "label"
     {:font "/builtins/fonts/default.font"
      :material "/builtins/fonts/label-df.material"}

     "material"
     {:attributes vertex-attribute}

     "model"
     {:materials {:name "material_name"
                  :material "/builtins/materials/model.material"
                  :textures {:sampler "tex0"
                             :texture image-proj-path}
                  :attributes vertex-attribute}}

     "particlefx"
     {:emitters {:modifiers {:properties (required {:points {:y 0.0}})}
                 :attributes vertex-attribute}
      :modifiers {:properties (required {:points (required {:y 0.0})})}}

     "sprite"
     {:attributes vertex-attribute
      :textures {:sampler "texture_sampler"
                 :texture image-proj-path}}

     "tileset"
     tile-source

     "tilesource"
     tile-source

     "tpatlas"
     {:file "/editable/depth01.tpinfo"}

     "tpinfo"
     {:pages {:name "tpinfo_page_image.png"
              :size {:width 16.0
                     :height 16.0}}}}))

;; This is the limit to how deeply we iterate into nested protobuf messages.
;; Ten is plenty. Currently, our deepest file formats only reach depth04.
(def ^:private ^:const max-pb-map-depth 10)

(defn- resource-type->pb-class
  ^Class [resource-type]
  (-> resource-type :test-info :ddf-type))

(defn- relevant-protobuf-resource-type? [resource-type]
  (and (ifn? (:load-fn resource-type))
       (class? (resource-type->pb-class resource-type))))

(defn- sparse-pb-map [^Class pb-class pb-path ^long depth-limit]
  (->> pb-class
       (protobuf/field-infos)
       (into {}
             (keep
               (fn [[field-key {:keys [default field-rule field-type-key options type]}]]
                 (let [pb-path (conj pb-path field-key)

                       [specific-value is-required]
                       (unwrap-pb-field-value (get-in pb-field-values pb-path))

                       value
                       (cond
                         (exactly? specific-value)
                         (unwrap-exactly specific-value)

                         (or is-required
                             (= :required field-rule)
                             (and (pos? depth-limit)
                                  (not (:runtime-only options))))
                         (cond
                           (= :message field-type-key)
                           (sparse-pb-map type pb-path (dec depth-limit))

                           (some? specific-value)
                           specific-value

                           (= :required field-rule)
                           default))]

                   (when (some? value)
                     (pair field-key
                           (case field-rule
                             :repeated (vector value)
                             value)))))))))

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
        (test-util/distinct-resource-types-by-editability workspace relevant-protobuf-resource-type?)))

(defn- with-absolute-file-keys [^File project-directory values-by-proj-path]
  (into (sorted-map)
        (map (fn [[proj-path content]]
               (let [relative-path (subs proj-path 1) ; Strip leading slash.
                     absolute-file (io/file project-directory relative-path)]
                 (pair absolute-file content))))
        values-by-proj-path))

(defn- create-parent-directories! [sparse-protobuf-content-by-absolute-file]
  (let [absolute-directories
        (into (sorted-set)
              (keep (fn [[^File absolute-file]]
                      (.getParentFile absolute-file)))
              sparse-protobuf-content-by-absolute-file)]

    ;; Create parent directories in the project.
    (doseq [directory absolute-directories]
      (fs/create-directories! directory))))

(defn- write-sparse-protobuf-resources! [sparse-protobuf-content-by-absolute-file]
  (doseq [[absolute-file content] sparse-protobuf-content-by-absolute-file]
    (test-support/spit-until-new-mtime absolute-file content)))

(deftest sparse-pb-map-meta-test
  (testing "go"
    (is (= {}
           (sparse-pb-map GameObject$PrototypeDesc ["go"] 0)))
    (is (= {:components [{:id "component_id"
                          :component "/referenced/prop.script"}]
            :embedded-components [{:id "embedded_component_id"
                                   :type "factory"
                                   :data "prototype: \"/referenced/instanced.go\"\n"}]}
           (sparse-pb-map GameObject$PrototypeDesc ["go"] 1)))
    (is (= {:components [{:id "component_id"
                          :component "/referenced/prop.script"
                          :position {}
                          :rotation {}
                          :scale {}
                          :properties [{:id "property_id"
                                        :type :property-type-number
                                        :value "1.0"}]}]
            :embedded-components [{:id "embedded_component_id"
                                   :type "factory"
                                   :data "prototype: \"/referenced/instanced.go\"\n"
                                   :position {}
                                   :rotation {}
                                   :scale {}}]}
           (sparse-pb-map GameObject$PrototypeDesc ["go"] 2))))

  (testing "collection"
    (is (= {:name ""}
           (sparse-pb-map GameObject$CollectionDesc ["collection"] 0)))
    (is (= {:name ""
            :instances [{:id "instance_id"
                         :prototype "/referenced/instanced.go"}]
            :embedded-instances [{:id "instance_id"
                                  :data (protobuf/map->str
                                          GameObject$PrototypeDesc
                                          {:components [{:id "component_id"
                                                         :component "/referenced/prop.script"
                                                         :properties [{:id "property_id"
                                                                       :type :property-type-number
                                                                       :value "1.0"}]}]
                                           :embedded-components [{:id "embedded_component_id"
                                                                  :type "factory"
                                                                  :data "prototype: \"/referenced/instanced.go\"\n"}]})}]
            :collection-instances [{:id "collection_instance_id"
                                    :collection "/referenced/instanced.collection"}]}
           (sparse-pb-map GameObject$CollectionDesc ["collection"] 1)))
    (is (= {:name ""
            :instances [{:id "instance_id"
                         :position {}
                         :rotation {}
                         :scale3 {}
                         :prototype "/referenced/instanced.go"
                         :component-properties [{:id "component_id"
                                                 :properties [{:id "property_id"
                                                               :type :property-type-number
                                                               :value "1.0"}]}]}]
            :embedded-instances [{:id "instance_id"
                                  :position {}
                                  :rotation {}
                                  :scale3 {}
                                  :data (protobuf/map->str
                                          GameObject$PrototypeDesc
                                          {:components [{:id "component_id"
                                                         :component "/referenced/prop.script"
                                                         :properties [{:id "property_id"
                                                                       :type :property-type-number
                                                                       :value "1.0"}]}]
                                           :embedded-components [{:id "embedded_component_id"
                                                                  :type "factory"
                                                                  :data "prototype: \"/referenced/instanced.go\"\n"}]})}]
            :collection-instances [{:id "collection_instance_id"
                                    :position {}
                                    :rotation {}
                                    :scale3 {}
                                    :collection "/referenced/instanced.collection"
                                    :instance-properties [{:id "instance_id"
                                                           :properties [{:id "component_id"
                                                                         :properties [{:id "property_id"
                                                                                       :type :property-type-number
                                                                                       :value "1.0"}]}]}]}]}
           (sparse-pb-map GameObject$CollectionDesc ["collection"] 2))))

  (testing "collisionobject"
    (is (= {:type :collision-object-type-dynamic
            :group ""
            :mass 0.0
            :friction 0.0
            :restitution 0.0}
           (sparse-pb-map Physics$CollisionObjectDesc ["collisionbobject"] 0)))
    (is (= {:type :collision-object-type-dynamic
            :group ""
            :mass 0.0
            :friction 0.0
            :restitution 0.0
            :embedded-collision-shape {}}
           (sparse-pb-map Physics$CollisionObjectDesc ["collisionbobject"] 1)))
    (is (= {:type :collision-object-type-dynamic
            :group ""
            :mass 0.0
            :friction 0.0
            :restitution 0.0
            :embedded-collision-shape {:data [1.0]
                                       :shapes [{:shape-type :type-sphere
                                                 :position {}
                                                 :rotation {}
                                                 :index 0
                                                 :count 1}]}}
           (sparse-pb-map Physics$CollisionObjectDesc ["collisionobject"] 2))))

  (testing "model"
    (is (= {:mesh ""}
           (sparse-pb-map ModelProto$ModelDesc ["model"] 0)))
    (is (= {:mesh ""
            :materials [{:name "material_name"
                         :material "/builtins/materials/model.material"}]}
           (sparse-pb-map ModelProto$ModelDesc ["model"] 1)))
    (is (= {:mesh ""
            :materials [{:name "material_name"
                         :material "/builtins/materials/model.material"
                         :textures [{:sampler "tex0"
                                     :texture "/builtins/graphics/particle_blob.png"}]
                         :attributes [{:name "attribute_name"
                                       :double-values {:v [0.0]}}]}]}
           (sparse-pb-map ModelProto$ModelDesc ["model"] 2)))))

(defn- write-save-value! [workspace ^String proj-path save-value]
  (let [resource (workspace/file-resource workspace proj-path)
        write-fn (:write-fn (resource/resource-type resource))
        content (write-fn save-value)]
    (test-support/spit-until-new-mtime resource content)))

(defn write-supplemental-files! [workspace save-values-by-proj-path]
  (doseq [[proj-path pb-map] (sort-by key save-values-by-proj-path)]
    (write-save-value! workspace proj-path pb-map)))

(deftest sparse-protobuf-test
  (let [project-path (test-util/make-temp-project-copy! "test/resources/empty_project")]
    (with-open [_ (test-util/make-directory-deleter project-path)]
      (test-util/set-non-editable-directories! project-path [(str "/" (name :non-editable))])

      (test-support/with-clean-system
        (let [workspace (test-util/setup-workspace! world project-path)]

          ;; Add dependencies to all sanctioned extensions to game.project.
          (test-util/set-libraries! workspace test-util/sanctioned-extension-urls)

          ;; With the extensions added, we can populate the workspace.
          (let [project-directory (workspace/project-directory workspace)
                sparse-protobuf-content-by-proj-path (sparse-protobuf-content-by-proj-path workspace)
                sparse-protobuf-content-by-absolute-file (with-absolute-file-keys project-directory sparse-protobuf-content-by-proj-path)
                supplemental-save-values-by-absolute-file (with-absolute-file-keys project-directory supplemental-save-values-by-proj-path)]

            ;; Ensure we have the parent directories in place for our content.
            (create-parent-directories! sparse-protobuf-content-by-absolute-file)
            (create-parent-directories! supplemental-save-values-by-absolute-file)

            ;; Add supplemental content to the project for our sparse protobuf
            ;; resources to reference.
            (write-supplemental-files! workspace supplemental-save-values-by-proj-path)
            (fs/copy! (io/file "test/resources/images/small.png")
                      (io/file project-path "editable/tpinfo_page_image.png"))

            ;; Add sparse files for all the protobuf resource types.
            (write-sparse-protobuf-resources! sparse-protobuf-content-by-absolute-file)

            ;; With the workspace populated, we can load the project.
            (workspace/resource-sync! workspace)

            (let [project (test-util/setup-project! workspace)
                  proj-paths (sort-by (fn [^String proj-path]
                                        (pair (resource/filename->type-ext proj-path)
                                              proj-path))
                                      (keys sparse-protobuf-content-by-proj-path))]
              (test-util/clear-cached-save-data! project)
              (doseq [proj-path proj-paths]
                (let [resource (workspace/find-resource workspace proj-path)
                      resource-node (project/get-resource-node project resource)
                      {:keys [read-fn write-fn view-types]} (resource/resource-type resource)
                      openable-in-view-type? (into #{} (map :id) view-types)]
                  (testing proj-path
                    (is (not (g/error? (g/node-value resource-node :_properties))))
                    (when (openable-in-view-type? :cljfx-form-view)
                      (is (not (g/error? (g/node-value resource-node :form-data)))))
                    (when (openable-in-view-type? :scene)
                      (is (not (g/error? (g/node-value resource-node :scene)))))
                    (when (resource/editable? resource)
                      (let [save-data (g/node-value resource-node :save-data)]
                        (when (is (not (g/error? save-data)))
                          (when (:dirty save-data)
                            (let [save-value (:save-value save-data)
                                  save-text (resource-node/save-data-content save-data)
                                  read-value (read-fn resource)
                                  read-text (slurp resource)
                                  written-read-text (write-fn read-value)]
                              (test-util/check-value-equivalence! read-value save-value read-text)
                              (test-util/check-text-equivalence! written-read-text save-text read-text))))))))))))))))

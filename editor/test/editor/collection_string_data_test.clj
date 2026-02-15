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

(ns editor.collection-string-data-test
  (:require [clojure.test :refer :all]
            [editor.collection-string-data :as collection-string-data]
            [editor.protobuf :as protobuf])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [com.dynamo.gamesys.proto GameSystem$FactoryDesc]))

(set! *warn-on-reflection* true)

(def ^:private typed-embedded-component-exts
  ["collisionobject"
   "label"
   "sprite"
   "sound"
   "model"
   "factory"
   "collectionfactory"
   "particlefx"
   "camera"
   "collectionproxy"])

(defn- ext->embedded-component-resource-type [ext]
  (case ext
    "factory" {:read-fn (partial protobuf/read-map-without-defaults GameSystem$FactoryDesc)
               :write-fn (partial protobuf/map->str GameSystem$FactoryDesc)}
    "factoryx" {:read-fn (partial protobuf/read-map-without-defaults GameSystem$FactoryDesc)
                :write-fn (partial protobuf/map->str GameSystem$FactoryDesc)}
    nil))

(deftest typed-payload-round-trip-for-all-builtin-components
  (doseq [component-ext typed-embedded-component-exts]
    (testing component-ext
      (let [component-data {:component-ext component-ext}
            payload-field-key (keyword component-ext)
            decoded-component-desc {:id component-ext
                                    :type component-ext
                                    :data component-data}
            encoded-component-desc (collection-string-data/string-encode-embedded-component-desc ext->embedded-component-resource-type decoded-component-desc)
            round-tripped-component-desc (collection-string-data/string-decode-embedded-component-desc ext->embedded-component-resource-type encoded-component-desc)]
        (is (nil? (:data encoded-component-desc)))
        (is (= component-data
               (get encoded-component-desc payload-field-key)))
        (is (= decoded-component-desc round-tripped-component-desc))))))

(deftest legacy-string-round-trip-for-unknown-component-type
  (let [decoded-component-desc {:id "factory"
                                :type "factoryx"
                                :data {:prototype "/main/enemy.go"}}
        encoded-component-desc (collection-string-data/string-encode-embedded-component-desc ext->embedded-component-resource-type decoded-component-desc)
        round-tripped-component-desc (collection-string-data/string-decode-embedded-component-desc ext->embedded-component-resource-type encoded-component-desc)]
    (is (string? (:data encoded-component-desc)))
    (is (= "factoryx" (:type encoded-component-desc)))
    (is (= decoded-component-desc round-tripped-component-desc))))

(deftest legacy-embedded-instance-is-written-with-typed-prototype-and-typed-components
  (let [legacy-instance-desc {:id "go1"
                              :data (protobuf/map->str
                                      GameObject$PrototypeDesc
                                      {:embedded-components [{:id "factory"
                                                             :type "factory"
                                                             :data (protobuf/map->str
                                                                     GameSystem$FactoryDesc
                                                                     {:prototype "/main/enemy.go"})}]})}
        decoded-instance-desc (collection-string-data/string-decode-embedded-instance-desc ext->embedded-component-resource-type legacy-instance-desc)
        encoded-instance-desc (collection-string-data/string-encode-embedded-instance-desc ext->embedded-component-resource-type decoded-instance-desc)
        encoded-component-desc (-> encoded-instance-desc :prototype :embedded-components first)]
    (is (map? (:data decoded-instance-desc)))
    (is (nil? (:data encoded-instance-desc)))
    (is (map? (:prototype encoded-instance-desc)))
    (is (= {:prototype "/main/enemy.go"}
           (:factory encoded-component-desc)))
    (is (nil? (:data encoded-component-desc)))))

(deftest verify-accepts-typed-embedded-instance-payload
  (is (nil?
        (collection-string-data/verify-string-decoded-embedded-instance-desc!
          {:id "go1"
           :prototype {:components []}}
          nil))))

(deftest empty-embedded-instance-payload-is-decoded-as-empty-prototype
  (doseq [encoded-instance-desc [{:id "go1" :data ""}
                                 {:id "go2"}]]
    (let [decoded-instance-desc (collection-string-data/string-decode-embedded-instance-desc
                                  ext->embedded-component-resource-type
                                  encoded-instance-desc)]
      (is (= {} (:data decoded-instance-desc)))
      (is (nil?
            (collection-string-data/verify-string-decoded-embedded-instance-desc!
              decoded-instance-desc
              nil))))))

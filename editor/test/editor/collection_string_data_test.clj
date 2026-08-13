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
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc GameObjectSource$PrototypeDesc]
           [com.dynamo.gamesys.proto GameSystem$FactoryDesc Sprite$SpriteDesc]
           [java.io StringReader]))

(set! *warn-on-reflection* true)

(defn- read-pb-map [pb-class sanitize-pb-map-fn ^StringReader reader]
  (sanitize-pb-map-fn (protobuf/read-map-without-defaults pb-class reader)))

(defn- write-pb-map [pb-class encode-pb-map-fn pb-map]
  (protobuf/map->str pb-class (encode-pb-map-fn pb-map)))

(def ^:private sprite-resource-type
  (let [sanitize-pb-map-fn #(if (#{"legacy" "encoded"} (:default-animation %))
                              (assoc % :default-animation "sanitized")
                              %)
        encode-pb-map-fn #(if (= "sanitized" (:default-animation %))
                            (assoc % :default-animation "encoded")
                            %)]
    {:ddf-type Sprite$SpriteDesc
     :sanitize-pb-map-fn sanitize-pb-map-fn
     :encode-pb-map-fn encode-pb-map-fn
     :read-fn (partial read-pb-map Sprite$SpriteDesc sanitize-pb-map-fn)
     :write-fn (partial write-pb-map Sprite$SpriteDesc encode-pb-map-fn)}))

(def ^:private spinemodel-resource-type
  {:ddf-type GameSystem$FactoryDesc
   :sanitize-pb-map-fn identity
   :encode-pb-map-fn identity
   :read-fn (partial read-pb-map GameSystem$FactoryDesc identity)
   :write-fn (partial write-pb-map GameSystem$FactoryDesc identity)})

(def ^:private ext->resource-type
  {"spinemodel" spinemodel-resource-type
   "sprite" sprite-resource-type})

(deftest embedded-component-source-decode-test
  (testing "Legacy strings and typed messages decode to the same canonical map."
    (let [expected {:id "sprite"
                    :type "sprite"
                    :data {:default-animation "sanitized"}}]
      (is (= expected
             (collection-string-data/source-decode-embedded-component-desc
               ext->resource-type
               {:id "sprite"
                :type "sprite"
                :data "default_animation: \"legacy\"\n"})))
      (is (= expected
             (collection-string-data/source-decode-embedded-component-desc
               ext->resource-type
               {:id "sprite"
                :type "sprite"
                :sprite {:default-animation "legacy"}})))))

  (testing "A selected empty message remains distinct from a missing payload."
    (is (= {:id "sprite"
            :type "sprite"
            :data {}}
           (collection-string-data/source-decode-embedded-component-desc
             ext->resource-type
             {:id "sprite"
              :type "sprite"
              :sprite {}})))
    (is (thrown-with-msg?
          clojure.lang.ExceptionInfo
          #"payload"
          (collection-string-data/source-decode-embedded-component-desc
            ext->resource-type
            {:id "sprite"
             :type "sprite"}))))

  (testing "The selected typed arm must match the retained component type."
    (is (thrown-with-msg?
          clojure.lang.ExceptionInfo
          #"sprite"
          (collection-string-data/source-decode-embedded-component-desc
            ext->resource-type
            {:id "sprite"
             :type "sprite"
             :label {}})))))

(deftest embedded-component-source-encode-test
  (testing "Built-in components use typed payloads and their map encoder."
    (is (= {:id "sprite"
            :type "sprite"
            :sprite {:default-animation "encoded"}}
           (collection-string-data/source-encode-embedded-component-desc
             ext->resource-type
             {:id "sprite"
              :type "sprite"
              :data {:default-animation "sanitized"}}))))

  (testing "Extension components remain on the legacy string fallback."
    (let [source-desc (collection-string-data/source-encode-embedded-component-desc
                        ext->resource-type
                        {:id "spine"
                         :type "spinemodel"
                         :data {:prototype "/spine/spine.go"}})]
      (is (= {:id "spine"
              :type "spinemodel"
              :data "prototype: \"/spine/spine.go\"\n"}
             source-desc))
      (is (not (contains? source-desc :spinemodel))))))

(deftest legacy-component-migrates-to-typed-source-test
  (is (= {:id "sprite"
          :type "sprite"
          :sprite {:default-animation "encoded"}}
         (->> {:id "sprite"
               :type "sprite"
               :data "default_animation: \"legacy\"\n"}
              (collection-string-data/source-decode-embedded-component-desc ext->resource-type)
              (collection-string-data/source-encode-embedded-component-desc ext->resource-type)))))

(deftest embedded-instance-source-roundtrip-test
  (let [canonical-instance
        {:id "go"
         :data {:embedded-components [{:id "sprite"
                                       :type "sprite"
                                       :data {:default-animation "sanitized"}}
                                      {:id "spine"
                                       :type "spinemodel"
                                       :data {:prototype "/spine/spine.go"}}]}}

        source-instance
        (collection-string-data/source-encode-embedded-instance-desc
          ext->resource-type
          canonical-instance)]
    (testing "Embedded game objects use a prototype message with nested typed and fallback payloads."
      (is (not (contains? source-instance :data)))
      (is (= {:id "sprite"
              :type "sprite"
              :sprite {:default-animation "encoded"}}
             (get-in source-instance [:prototype :embedded-components 0])))
      (is (= "prototype: \"/spine/spine.go\"\n"
             (get-in source-instance [:prototype :embedded-components 1 :data]))))

    (testing "Typed source survives protobuf text serialization, including its oneof selections."
      (let [source-prototype (:prototype source-instance)
            source-text (protobuf/map->str GameObjectSource$PrototypeDesc source-prototype)
            parsed-source-prototype (protobuf/str->map-without-defaults GameObjectSource$PrototypeDesc source-text)]
        (is (contains? (get-in parsed-source-prototype [:embedded-components 0]) :sprite))
        (is (contains? (get-in parsed-source-prototype [:embedded-components 1]) :data))))

    (testing "Decoding restores the canonical editor representation."
      (is (= canonical-instance
             (-> source-instance
                 (collection-string-data/source-decode-embedded-instance-desc)
                 (update :data (partial collection-string-data/source-decode-prototype-desc
                                        ext->resource-type))))))))

(deftest empty-embedded-instance-prototype-test
  (let [source-instance {:id "empty"
                         :prototype {}}
        canonical-instance {:id "empty"
                            :data {}}]
    (is (= canonical-instance
           (collection-string-data/source-decode-embedded-instance-desc
             source-instance)))
    (is (= source-instance
           (collection-string-data/source-encode-embedded-instance-desc
             ext->resource-type
             canonical-instance)))))

(deftest legacy-collection-migrates-to-typed-source-test
  (let [legacy-prototype-text
        (protobuf/map->str
          GameObjectSource$PrototypeDesc
          {:embedded-components [{:id "sprite"
                                  :type "sprite"
                                  :data "default_animation: \"legacy\"\n"}]})

        canonical-collection
        (collection-string-data/source-decode-collection-desc
          ext->resource-type
          {:name "main"
           :embedded-instances [{:id "go"
                                 :data legacy-prototype-text}]})

        source-collection
        (collection-string-data/source-encode-collection-desc
          ext->resource-type
          canonical-collection)]
    (is (= {:default-animation "sanitized"}
           (get-in canonical-collection [:embedded-instances 0 :data :embedded-components 0 :data])))
    (is (= {:default-animation "encoded"}
           (get-in source-collection [:embedded-instances 0 :prototype :embedded-components 0 :sprite])))
    (is (not (contains? (get-in source-collection [:embedded-instances 0]) :data)))))

(deftest collection-source-encode-test
  (let [source-collection
        (collection-string-data/source-encode-collection-desc
          ext->resource-type
          {:name "main"
           :embedded-instances [{:id "go"
                                 :data {:embedded-components [{:id "sprite"
                                                               :type "sprite"
                                                               :data {}}]}}]})]
    (is (= {}
           (get-in source-collection [:embedded-instances 0 :prototype :embedded-components 0 :sprite])))
    (is (not (contains? (get-in source-collection [:embedded-instances 0]) :data)))))

(deftest legacy-string-codec-compatibility-test
  (let [canonical-prototype
        {:embedded-components [{:id "sprite"
                                :type "sprite"
                                :data {:default-animation "sanitized"}}]}

        string-encoded-prototype
        (collection-string-data/string-encode-prototype-desc ext->resource-type canonical-prototype)

        string-encoded-instance
        (collection-string-data/string-encode-embedded-instance-desc
          ext->resource-type
          {:id "go"
           :data canonical-prototype})]
    (is (string? (get-in string-encoded-prototype [:embedded-components 0 :data])))
    (is (not (contains? (get-in string-encoded-prototype [:embedded-components 0]) :sprite)))
    (is (= canonical-prototype
           (collection-string-data/string-decode-prototype-desc ext->resource-type string-encoded-prototype)))
    (is (string? (:data string-encoded-instance)))
    (is (= string-encoded-prototype
           (protobuf/str->map-without-defaults GameObject$PrototypeDesc (:data string-encoded-instance))))))

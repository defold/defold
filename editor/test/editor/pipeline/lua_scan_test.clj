(ns editor.pipeline.lua-scan-test
  (:require [clojure.test :refer :all]
            [editor.pipeline.lua-scan :refer :all]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [support.test-support :as test-support]
            [editor.properties :as properties]))

(deftest src->properties-test
  (test-support/with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          base-resource (workspace/resolve-workspace-resource workspace "/script/props.script")]
      (is (= [{:name      "test"
               :type      :property-type-number
               :sub-type  0
               :raw-value "1.1"
               :value     1.1
               :line      0
               :status    :ok}]
             (src->properties base-resource "go.property(\"test\",1.1)")))

      (is (= [{:value true
               :type :property-type-boolean
               :sub-type 0}
              {:value "aBc3"
               :type :property-type-hash
               :sub-type 0}
              {:value "foo"
               :type :property-type-url
               :sub-type 0}
              {:value [1.0 2.0 3.0]
               :type :property-type-vector3
               :sub-type 0}
              {:value [1.0 2.0 3.0 4.0]
               :type :property-type-vector4
               :sub-type 0}
              {:value [0.0 28.072486935852957 90.0]
               :type :property-type-quat
               :sub-type 0}
              {:value nil
               :type :property-type-resource
               :sub-type properties/sub-type-material}
              {:value nil
               :type :property-type-resource
               :sub-type properties/sub-type-material}
              {:value (workspace/resolve-workspace-resource workspace "/absolute/path/to/resource.material")
               :type :property-type-resource
               :sub-type properties/sub-type-material}
              {:value (workspace/resolve-workspace-resource workspace "/script/relative/path/to/resource.material")
               :type :property-type-resource
               :sub-type properties/sub-type-material}
              {:value nil
               :type :property-type-resource
               :sub-type properties/sub-type-texture}
              {:value nil
               :type :property-type-resource
               :sub-type properties/sub-type-texture}
              {:value (workspace/resolve-workspace-resource workspace "/absolute/path/to/resource.atlas")
               :type :property-type-resource
               :sub-type properties/sub-type-texture}
              {:value (workspace/resolve-workspace-resource workspace "/script/relative/path/to/resource.atlas")
               :type :property-type-resource
               :sub-type properties/sub-type-texture}
              {:value nil
               :type :property-type-resource
               :sub-type properties/sub-type-textureset}
              {:value nil
               :type :property-type-resource
               :sub-type properties/sub-type-textureset}
              {:value (workspace/resolve-workspace-resource workspace "/absolute/path/to/resource.tilesource")
               :type :property-type-resource
               :sub-type properties/sub-type-textureset}
              {:value (workspace/resolve-workspace-resource workspace "/script/relative/path/to/resource.tilesource")
               :type :property-type-resource
               :sub-type properties/sub-type-textureset}]
             (map #(select-keys (first %) [:value :type :sub-type])
                  [(src->properties base-resource "go.property(\"test\",true)")
                   (src->properties base-resource "go.property(\"test\",hash ('aBc3'))")
                   (src->properties base-resource "go.property(\"test\",msg.url ('foo'))")
                   (src->properties base-resource "go.property(\"test\",vmath.vector3 (1,2,3))")
                   (src->properties base-resource "go.property(\"test\",vmath.vector4 (1,2,3,4))")
                   (src->properties base-resource "go.property(\"test\",vmath.quat (1,2,3,4))")
                   (src->properties base-resource "go.property(\"test\",material ())")
                   (src->properties base-resource "go.property(\"test\",material (''))")
                   (src->properties base-resource "go.property(\"test\",material ('/absolute/path/to/resource.material'))")
                   (src->properties base-resource "go.property(\"test\",material ('relative/path/to/resource.material'))")
                   (src->properties base-resource "go.property(\"test\",texture ())")
                   (src->properties base-resource "go.property(\"test\",texture (''))")
                   (src->properties base-resource "go.property(\"test\",texture ('/absolute/path/to/resource.atlas'))")
                   (src->properties base-resource "go.property(\"test\",texture ('relative/path/to/resource.atlas'))")
                   (src->properties base-resource "go.property(\"test\",textureset ())")
                   (src->properties base-resource "go.property(\"test\",textureset (''))")
                   (src->properties base-resource "go.property(\"test\",textureset ('/absolute/path/to/resource.tilesource'))")
                   (src->properties base-resource "go.property(\"test\",textureset ('relative/path/to/resource.tilesource'))")])))

      (is (= [] (src->properties base-resource "foo.property(\"test\",true)")))

      (is (= [] (src->properties base-resource "go.property")))
      (is (= [{:name      nil
               :type      nil
               :sub-type  0
               :raw-value nil
               :value     nil
               :line      0
               :status    :invalid-args}]
             (src->properties base-resource "go.property()")
             (src->properties base-resource "go.property(\"test\")"))))))

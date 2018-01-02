(ns editor.pipeline.lua-scan-test
  (:require [clojure.test :refer :all]
            [editor.pipeline.lua-scan :refer :all]))

(deftest src->properties-test
  (is (= [{:name      "test"
           :type      :property-type-number
           :raw-value "1.1"
           :value     1.1
           :line      0
           :status    :ok}]
         (src->properties "go.property(\"test\",1.1)")))

  (is (= [{:value true
           :type  :property-type-boolean}
          {:value "aBc3"
           :type  :property-type-hash}
          {:value "foo"
           :type  :property-type-url}
          {:value [1.0 2.0 3.0]
           :type  :property-type-vector3}
          {:value [1.0 2.0 3.0 4.0]
           :type  :property-type-vector4}
          {:value [0.0 28.072486935852957 90.0]
           :type   :property-type-quat}
          {:value ""
           :type  :property-type-resource}
          {:value ""
           :type  :property-type-resource}
          {:value "/path/to/resource.material"
           :type  :property-type-resource}
          {:value ""
           :type  :property-type-resource}
          {:value ""
           :type  :property-type-resource}
          {:value "/path/to/resource.atlas"
           :type  :property-type-resource}
          {:value ""
           :type  :property-type-resource}
          {:value ""
           :type  :property-type-resource}
          {:value "/path/to/resource.tilesource"
           :type  :property-type-resource}]
         (map #(select-keys (first %) [:value :type])
              [(src->properties "go.property(\"test\",true)")
               (src->properties "go.property(\"test\",hash ('aBc3'))")
               (src->properties "go.property(\"test\",msg.url ('foo'))")
               (src->properties "go.property(\"test\",vmath.vector3 (1,2,3))")
               (src->properties "go.property(\"test\",vmath.vector4 (1,2,3,4))")
               (src->properties "go.property(\"test\",vmath.quat (1,2,3,4))")
               (src->properties "go.property(\"test\",material ())")
               (src->properties "go.property(\"test\",material (''))")
               (src->properties "go.property(\"test\",material ('/path/to/resource.material'))")
               (src->properties "go.property(\"test\",texture ())")
               (src->properties "go.property(\"test\",texture (''))")
               (src->properties "go.property(\"test\",texture ('/path/to/resource.atlas'))")
               (src->properties "go.property(\"test\",textureset ())")
               (src->properties "go.property(\"test\",textureset (''))")
               (src->properties "go.property(\"test\",textureset ('/path/to/resource.tilesource'))")])))

  (is (= [] (src->properties "foo.property(\"test\",true)")))

  (is (= [] (src->properties "go.property")))
  (is (= [{:name      nil
           :type      nil
           :raw-value nil
           :value     nil
           :line      0
           :status    :invalid-args}]
         (src->properties "go.property()")
         (src->properties "go.property(\"test\")"))))

(ns dynamo.image-test
  (:require [dynamo.geom :refer :all]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [clojure.java.io :refer [as-url file]]
            [schema.test]
            [dynamo.image :refer :all])
  (:import [java.awt.image BufferedImage]))

(use-fixtures :once schema.test/validate-schemas)

(deftest image-loading
  (let [img (make-image (as-url (file "foo")) (BufferedImage. 128 192 BufferedImage/TYPE_4BYTE_ABGR))]
    (is (= 128 (.width img)))
    (is (= 192 (.height img)))))

#_(run-tests)
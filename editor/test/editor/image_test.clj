(ns editor.image-test
  (:require [clojure.java.io :refer [as-url file]]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [editor.image :refer :all]
            [editor.image-util :refer :all]
            [editor.geom :refer :all]
            [schema.test])
  (:import [java.awt.image BufferedImage]))

(use-fixtures :once schema.test/validate-schemas)

(deftest image-loading
  (let [img (make-image (as-url (file "foo")) (BufferedImage. 128 192 BufferedImage/TYPE_4BYTE_ABGR))]
    (is (= 128 (.width img)))
    (is (= 192 (.height img)))))

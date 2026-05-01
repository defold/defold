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

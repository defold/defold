;; Copyright 2020 The Defold Foundation
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

(ns editor.prefs-test
  (:require [clojure.test :refer :all]
            [editor.prefs :as prefs])
  (:import [javafx.scene.paint Color]))

(deftest prefs-test
  (let [p (prefs/make-prefs "unit-test")
        c (Color/web "#11223344")]
    (prefs/set-prefs p "foo" [1 2 3])
    (prefs/set-prefs p "color" c)
    (is (= [1 2 3] (prefs/get-prefs p "foo" nil)))
    (is (= "unknown" (prefs/get-prefs p "___----does-not-exists" "unknown")))
    (is (= c (prefs/get-prefs p "color" nil)))))

(deftest prefs-load-test
  (let [p (prefs/load-prefs "test/resources/test_prefs.json")]
    (is (= "default" (prefs/get-prefs p "missing" "default")))
    (is (= "bar" (prefs/get-prefs p "foo" "default")))
    (prefs/set-prefs p "foo" "bar2")
    (is (= "bar2" (prefs/get-prefs p "foo" "default")))))

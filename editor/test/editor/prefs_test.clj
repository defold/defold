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
    (is (= "unknown" (prefs/get-prefs p "___----does-not-exists" "unknown" )))
    (is (= c (prefs/get-prefs p "color" nil)))))

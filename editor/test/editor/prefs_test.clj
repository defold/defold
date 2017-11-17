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

(deftest prefs-load-test
  (let [p (prefs/load-prefs "test/resources/test_prefs.json")]
    (is (= "default" (prefs/get-prefs p "missing" "default")))
    (is (= "bar" (prefs/get-prefs p "foo" "default")))
    (prefs/set-prefs p "foo" "bar2")
    (is (= "bar2" (prefs/get-prefs p "foo" "default")))))

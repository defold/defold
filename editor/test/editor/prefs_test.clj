(ns editor.prefs-test
  (:require [clojure.test :refer :all]
            [editor.prefs :as prefs]))

(deftest prefs-test
  (let [p (prefs/make-prefs "unit-test")]
    (prefs/set-prefs p "foo" [1 2 3])
    (is (= [1 2 3] (prefs/get-prefs p "foo" nil)))
    (is (= "unknown" (prefs/get-prefs p "___----does-not-exists" "unknown" )))))


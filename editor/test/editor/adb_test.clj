(ns editor.adb-test
  (:require [clojure.test :refer :all]
            [editor.adb :as adb]))

(deftest path-evaluator-test
  (are [expected actual] (= expected (#'adb/evaluate-path actual))
    ;; leading ~ is expanded
    (System/getProperty "user.home") "~"
    (str (System/getProperty "user.home") "/adb") "~/adb"
    ;; non-leading ~ is preserved
    "/usr/bin/~/adb" "/usr/bin/~/adb"
    ;; $ENV vars are expanded
    (System/getenv "USER") "$USER"
    (str "/home/" (System/getenv "USER") "/adb") "/home/$USER/adb"
    ;; absent env vars make the whole output collapse into nil
    nil "/home/$UHHH"))

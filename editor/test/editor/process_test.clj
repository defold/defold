(ns editor.process-test
  (:require [clojure.test :refer :all]
            [editor.process :as process])
  (:import [java.io ByteArrayOutputStream]))

(deftest start-test
  (let [p (process/start "ls")
        output (process/capture (process/out p))
        exit (process/await-exit-code p)]
    (is (process/exit-ok? exit))
    (is (not-empty output))))

(deftest exec-test
  (let [output (process/exec "ls")]
    (is (not-empty output))))

(deftest env-test
  (let [val "UHH"
        out (process/exec {:env {"CUSTOM_VAR" val}} "printenv" "CUSTOM_VAR")]
    (is (= val out))))

(deftest remove-env-test
  (is (not-empty (process/exec "printenv" "PATH")))
  (is (thrown? Exception (process/exec {:env {"PATH" nil}} "printenv" "PATH"))))

(deftest redirect-test
  (is (nil? (process/exec "bash" "-c" "ls >&2")))
  (is (not-empty (process/exec {:err :stdout} "bash" "-c" "ls >&2"))))

(deftest pipe-test
  (let [p (process/start "ls")
        target (ByteArrayOutputStream.)]
    (process/pipe (process/out p) target)
    (is (not-empty (str target)))))

(deftest on-exit-test
  (let [p (process/start "ls")
        done (promise)]
    (process/on-exit p #(deliver done true))
    (is (true? (deref done 100 false)))))
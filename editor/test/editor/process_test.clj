(ns editor.process-test
  (:require [clojure.test :refer :all]
            [editor.process :as process]
            [editor.util :as util])
  (:import [java.io ByteArrayOutputStream]))

(deftest start-test
  (when-not (util/is-win32?)
    (let [p (process/start! "ls")
          output (process/capture! (process/out p))
          exit (process/await-exit-code p)]
      (is (zero? exit))
      (is (not-empty output)))))

(deftest exec-test
  (when-not (util/is-win32?)
    (let [output (process/exec! "ls")]
      (is (not-empty output)))))

(deftest env-test
  (when-not (util/is-win32?)
    (let [val "UHH"
          out (process/exec! {:env {"CUSTOM_VAR" val}} "printenv" "CUSTOM_VAR")]
      (is (= val out)))))

(deftest remove-env-test
  (when-not (util/is-win32?)
    (is (not-empty (process/exec! "printenv" "PATH")))
    (is (thrown? Exception (process/exec! {:env {"PATH" nil}} "printenv" "PATH")))))

(deftest redirect-test
  (when-not (util/is-win32?)
    (is (nil? (process/exec! "bash" "-c" "ls >&2")))
    (is (not-empty (process/exec! {:err :stdout} "bash" "-c" "ls >&2")))))

(deftest pipe-test
  (when-not (util/is-win32?)
    (let [p (process/start! "ls")
          target (ByteArrayOutputStream.)]
      (process/pipe! (process/out p) target)
      (is (not-empty (str target))))))

(deftest on-exit-test
  (when-not (util/is-win32?)
    (let [p (process/start! "ls")
          done (promise)]
      (process/on-exit! p #(deliver done true))
      (is (true? (deref done 100 false))))))
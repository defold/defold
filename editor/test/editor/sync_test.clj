(ns editor.sync-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.git-test :refer :all]
            [editor.prefs :as prefs]
            [editor.git :as git]
            [editor.sync :as sync]
            [editor.progress :as progress])
  (:import [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]
           [org.eclipse.jgit.api Git]
           [org.apache.commons.io FileUtils]))

(defn- new-flow [git]
  (let [p (prefs/make-prefs "unit-test")]
    (prefs/set-prefs p "email" "foo@bar.com")
    (prefs/set-prefs p "token" "TOKEN")
    (sync/make-flow git p)))

(deftest make-flow-test
  (let [git  (new-git)
        flow (new-flow git)]
    (is (= :pull/start (:state flow)))
    (is (= git (:git flow)))
    (is (instance? org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider (:creds flow)))
    (is (instance? org.eclipse.jgit.revwalk.RevCommit (:start-ref flow)))
    (is (nil? (:stash-ref flow)))
    (delete-git git)))

(deftest cancel-flow-test
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (let [flow (new-flow git)]
      (create-file git "/src/main.cpp" "void main() {FOO}")
      (sync/cancel-flow flow)
      (is (= "void main() {}" (slurp-file git "/src/main.cpp"))))
    (delete-git git)))

(deftest advance-flow-test
  (testing ":pull/start"
    (let [git       (new-git)
          local-git (clone git)
          flow      (new-flow local-git)
          res       (sync/advance-flow flow progress/null-render-progress!)]
      (is (= :pull/done (:state res)))))

  ;; TODO -- filling in
  )

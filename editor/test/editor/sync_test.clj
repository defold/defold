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
  (testing ":pull/done"
    (let [git       (new-git)
          local-git (clone git)
          flow      (new-flow local-git)
          res       (sync/advance-flow flow progress/null-render-progress!)]
      (is (= :pull/done (:state res)))
      (delete-git git)
      (delete-git local-git)))

  (testing ":pull/conflicts"
    (let [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (let [local-git (clone git)]
        (create-file git "/src/main.cpp" "void main() {FOO}")
        (commit-src git)
        (create-file local-git "/src/main.cpp" "void main() {BAR}")
        (let [!flow (atom (new-flow local-git))
              res   (swap! !flow sync/advance-flow progress/null-render-progress!)]
          (is (= #{"src/main.cpp"} (:conflicting (git/status local-git))))
          (is (= :pull/conflicts (:state res)))
          (let [flow2 @!flow]
            (testing "theirs"
              (create-file local-git "/src/main.cpp" (sync/get-theirs @!flow "src/main.cpp"))
              (sync/resolve-file !flow "src/main.cpp")
              (is (= {"src/main.cpp" :both-modified} (:resolved @!flow)))
              (is (= #{"src/main.cpp"} (:staged @!flow)))
              (let [res2 (sync/advance-flow @!flow progress/null-render-progress!)]
                (is (= :pull/done (:state res2))))
              (is (= "void main() {FOO}" (slurp-file local-git "/src/main.cpp"))))
            (testing "ours"
              (reset! !flow flow2)
              (create-file local-git "/src/main.cpp" (sync/get-ours @!flow "src/main.cpp"))
              (sync/resolve-file !flow "src/main.cpp")
              (is (= {"src/main.cpp" :both-modified} (:resolved @!flow)))
              (is (= #{"src/main.cpp"} (:staged @!flow)))
              (let [res2 (sync/advance-flow @!flow progress/null-render-progress!)]
                (is (= :pull/done (:state res2))))
              (is (= "void main() {BAR}" (slurp-file local-git "/src/main.cpp"))))))
        (delete-git local-git))
      (delete-git git)))

  (testing ":push-staging -> :push/done"
    (let [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (let [local-git (clone git)
            !flow     (atom (assoc (new-flow local-git) :state :push/start))]
        (create-file local-git "/src/main.cpp" "void main() {BAR}")
        (swap! !flow sync/advance-flow progress/null-render-progress!)
        (is (= :push/staging (:state @!flow)))
        (git/stage-file local-git "src/main.cpp")
        (swap! !flow sync/refresh-git-state)
        (is (= #{"src/main.cpp"} (:staged @!flow)))
        (swap! !flow merge {:state   :push/comitting
                            :message "foobar"})
        (swap! !flow sync/advance-flow progress/null-render-progress!)
        (is (= :push/done (:state @!flow)))
        (delete-git git)
        (delete-git local-git)))))

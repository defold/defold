(ns editor.sync-test
  (:require [clojure.test :refer :all]
            [editor.git-test :refer :all]
            [editor.prefs :as prefs]
            [editor.git :as git]
            [editor.sync :as sync]
            [editor.progress :as progress]))

(def without-login-prompt
  (fn [f]
    (binding [sync/*login* false]
      (f))))

(use-fixtures :once without-login-prompt)

(defn- make-prefs []
  (let [p (prefs/make-prefs "unit-test")]
    (prefs/set-prefs p "email" "foo@bar.com")
    (prefs/set-prefs p "token" "TOKEN")
    p))

(defn- flow-equal? [a b]
  (and (= a (assoc b :creds (:creds a)))
       (= (instance? org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider (:creds a))
          (instance? org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider (:creds b)))))

(deftest flow-in-progress-test
  (is (false? (sync/flow-in-progress? nil)))
  (with-git [git (new-git)]
    (is (false? (sync/flow-in-progress? git)))))

(deftest begin-flow-test
  (with-git [git   (new-git)
             prefs (make-prefs)
             flow  @(sync/begin-flow! git prefs)]
    (is (= :pull/start (:state flow)))
    (is (= git (:git flow)))
    (is (instance? org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider (:creds flow)))
    (is (instance? org.eclipse.jgit.revwalk.RevCommit (:start-ref flow)))
    (is (nil? (:stash-ref flow)))
    (is (true? (sync/flow-in-progress? git)))
    (is (flow-equal? flow @(sync/resume-flow git prefs)))))

(deftest resume-flow-test
  (with-git [git (new-git)]
    (create-file git "/existing.txt" "A file that already existed in the repo.")
    (commit-src git)
    (create-file git "/existing.txt" "A file that already existed in the repo, with unstaged changes.")
    (create-file git "/added.txt" "A file that has been added but not staged.")
    (let [prefs (make-prefs)
          !flow (sync/begin-flow! git prefs)]
      (is (flow-equal? @!flow @(sync/resume-flow git prefs))))))

(deftest cancel-flow-test
  (with-git [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (let [!flow (sync/begin-flow! git (make-prefs))]
      (is (true? (sync/flow-in-progress? git)))
      (create-file git "/src/main.cpp" "void main() {FOO}")
      (sync/cancel-flow! !flow)
      (is (= "void main() {}" (slurp-file git "/src/main.cpp")))
      (is (false? (sync/flow-in-progress? git))))))

(deftest finish-flow-test
  (with-git [git (new-git)]
    (let [!flow (sync/begin-flow! git (make-prefs))]
      (is (true? (sync/flow-in-progress? git)))
      (sync/finish-flow! !flow)
      (is (false? (sync/flow-in-progress? git))))))

(deftest advance-flow-test
  (testing ":pull/done"
    (with-git [git       (new-git)
               local-git (clone git)
               prefs     (make-prefs)
               !flow     (sync/begin-flow! local-git prefs)]
      (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
      (swap! !flow sync/advance-flow progress/null-render-progress!)
      (is (= :pull/done (:state @!flow)))
      (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))))

  (testing ":pull/conflicts"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (with-git [local-git (clone git)]
        (create-file git "/src/main.cpp" "void main() {FOO}")
        (commit-src git)
        (create-file local-git "/src/main.cpp" "void main() {BAR}")
        (let [prefs (make-prefs)
              !flow (sync/begin-flow! local-git prefs)
              res   (swap! !flow sync/advance-flow progress/null-render-progress!)]
          (is (= #{"src/main.cpp"} (:conflicting (git/status local-git))))
          (is (= :pull/conflicts (:state res)))
          (is (flow-equal? res @(sync/resume-flow local-git prefs)))
          (let [flow2 @!flow]
            (testing "theirs"
              (create-file local-git "/src/main.cpp" (sync/get-theirs @!flow "src/main.cpp"))
              (sync/resolve-file! !flow "src/main.cpp")
              (is (= {"src/main.cpp" :both-modified} (:resolved @!flow)))
              (is (= #{} (:staged @!flow)))
              (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
              (swap! !flow sync/advance-flow progress/null-render-progress!)
              (is (= :pull/done (:state @!flow)))
              (is (= "void main() {FOO}" (slurp-file local-git "/src/main.cpp")))
              (is (flow-equal? @!flow @(sync/resume-flow local-git prefs))))
            (testing "ours"
              (reset! !flow flow2)
              (create-file local-git "/src/main.cpp" (sync/get-ours @!flow "src/main.cpp"))
              (sync/resolve-file! !flow "src/main.cpp")
              (is (= {"src/main.cpp" :both-modified} (:resolved @!flow)))
              (is (= #{} (:staged @!flow)))
              (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
              (swap! !flow sync/advance-flow progress/null-render-progress!)
              (is (= :pull/done (:state @!flow)))
              (is (= "void main() {BAR}" (slurp-file local-git "/src/main.cpp")))
              (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))))))))

  (testing ":push-staging -> :push/done"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (with-git [local-git (clone git)
                 prefs     (make-prefs)
                 !flow     (sync/begin-flow! local-git prefs)]
        (swap! !flow assoc :state :push/start)
        (create-file local-git "/src/main.cpp" "void main() {BAR}")
        (swap! !flow sync/advance-flow progress/null-render-progress!)
        (is (= :push/staging (:state @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
        (git/stage-change! local-git (git/make-modified-change "src/main.cpp"))
        (swap! !flow sync/refresh-git-state)
        (is (= #{"src/main.cpp"} (:staged @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
        (swap! !flow merge {:state   :push/comitting
                            :message "foobar"})
        (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
        (swap! !flow sync/advance-flow progress/null-render-progress!)
        (is (= :push/done (:state @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))))))

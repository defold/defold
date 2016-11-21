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

(deftest find-git-state-test
  (let [base-status {:added #{}
                     :changed #{}
                     :conflicting #{}
                     :conflicting-stage-state #{}
                     :ignored-not-in-index #{".DS_Store"
                                             ".internal"
                                             "build"
                                             "main/.DS_Store"
                                             "main/images/.DS_Store"}
                     :missing #{}
                     :modified #{}
                     :removed #{}
                     :uncommitted-changes #{}
                     :untracked #{}
                     :untracked-folders #{}}]
    (testing "Working directory clean"
      (is (= {:modified #{}
              :staged #{}}
             (sync/find-git-state base-status
                                  []))))
    (testing "Added file, unstaged"
      (is (= {:modified #{(git/make-add-change "main/added.script")}
              :staged #{}}
             (sync/find-git-state (merge base-status
                                         {:untracked #{"main/added.script"}})
                                  [{:change-type :add
                                    :new-path "main/added.script"
                                    :old-path nil
                                    :score 0}]))))
    (testing "Added file, staged"
      (is (= {:modified #{}
              :staged #{(git/make-add-change "main/added.script")}}
             (sync/find-git-state (merge base-status
                                         {:added #{"main/added.script"}
                                          :uncommited-changes #{"main/added.script"}})
                                  [{:change-type :add
                                    :new-path "main/added.script"
                                    :old-path nil
                                    :score 0}]))))
    (testing "Deleted file, unstaged"
      (is (= {:modified #{(git/make-delete-change "main/deleted.script")}
              :staged #{}}
             (sync/find-git-state (merge base-status
                                         {:missing #{"main/deleted.script"}
                                          :uncommited-changes #{"main/deleted.script"}})
                                  [{:change-type :delete
                                    :new-path nil
                                    :old-path "main/deleted.script"
                                    :score 0}]))))
    (testing "Deleted file, staged"
      (is (= {:modified #{}
              :staged #{(git/make-delete-change "main/deleted.script")}}
             (sync/find-git-state (merge base-status
                                         {:removed #{"main/deleted.script"}
                                          :uncommited-changes #{"main/deleted.script"}})
                                  [{:change-type :delete
                                    :new-path nil
                                    :old-path "main/deleted.script"
                                    :score 0}]))))
    (testing "Modified file, unstaged"
      (is (= {:modified #{(git/make-modify-change "main/modified.script")}
              :staged #{}}
             (sync/find-git-state (merge base-status
                                         {:modified #{"main/modified.script"}
                                          :uncommited-changes #{"main/modified.script"}})
                                  [{:change-type :modify
                                    :new-path "main/modified.script"
                                    :old-path "main/modified.script"
                                    :score 0}]))))
    (testing "Modified file, staged"
      (is (= {:modified #{}
              :staged #{(git/make-modify-change "main/modified.script")}}
             (sync/find-git-state (merge base-status
                                         {:changed #{"main/modified.script"}
                                          :uncommited-changes #{"main/modified.script"}})
                                  [{:change-type :modify
                                    :new-path "main/modified.script"
                                    :old-path "main/modified.script"
                                    :score 0}]))))
    (testing "Renamed file, unstaged"
      (is (= {:modified #{(git/make-rename-change "main/old-name.script" "main/new-name.script")}
              :staged #{}}
             (sync/find-git-state (merge base-status
                                         {:missing #{"main/old-name.script"}
                                          :uncommited-changes #{"main/old-name.script"}
                                          :untracked #{"main/new-name.script"}})
                                  [{:change-type :rename
                                    :new-path "main/new-name.script"
                                    :old-path "main/old-name.script"
                                    :score 100}]))))
    (testing "Renamed file, staged"
      (is (= {:modified #{}
              :staged #{(git/make-rename-change "main/old-name.script" "main/new-name.script")}}
             (sync/find-git-state (merge base-status
                                         {:added #{"main/new-name.script"}
                                          :removed #{"main/old-name.script"}
                                          :uncommited-changes #{"main/old-name.script" "main/new-name.script"}})
                                  [{:change-type :rename
                                    :new-path "main/new-name.script"
                                    :old-path "main/old-name.script"
                                    :score 100}]))))))

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
        (git/stage-change! local-git (git/make-modify-change "src/main.cpp"))
        (swap! !flow sync/refresh-git-state)
        (is (= #{(git/make-modify-change "src/main.cpp")} (:staged @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
        (swap! !flow merge {:state   :push/committing
                            :message "foobar"})
        (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))
        (swap! !flow sync/advance-flow progress/null-render-progress!)
        (is (= :push/done (:state @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git prefs)))))))

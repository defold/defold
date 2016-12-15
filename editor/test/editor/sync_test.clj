(ns editor.sync-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.git-test :refer :all]
            [editor.prefs :as prefs]
            [editor.git :as git]
            [editor.sync :as sync]
            [editor.progress :as progress]
            [integration.test-util :as test-util]))

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
                                    :score 100}]))))
    (testing "Renamed file, only add staged"
      (is (= {:modified #{(git/make-delete-change "main/old-name.script")}
              :staged #{(git/make-add-change "main/new-name.script")}}
             (sync/find-git-state (merge base-status
                                         {:added #{"main/new-name.script"}
                                          :missing #{"main/old-name.script"}
                                          :uncommited-changes #{"main/old-name.script" "main/new-name.script"}})
                                  [{:change-type :rename
                                    :new-path "main/new-name.script"
                                    :old-path "main/old-name.script"
                                    :score 100}]))))
    (testing "Renamed file, only delete staged"
      (is (= {:modified #{(git/make-add-change "main/new-name.script")}
              :staged #{(git/make-delete-change "main/old-name.script")}}
             (sync/find-git-state (merge base-status
                                         {:removed #{"main/old-name.script"}
                                          :uncommited-changes #{"main/old-name.script" "main/new-name.script"}
                                          :untracked #{"main/new-name.script"}})
                                  [{:change-type :rename
                                    :new-path "main/new-name.script"
                                    :old-path "main/old-name.script"
                                    :score 100}]))))
    (testing "Renamed and modified file, unstaged"
      (is (= {:modified #{(git/make-delete-change "main/old-name.script")
                          (git/make-add-change "main/new-name.script")}
              :staged #{}}
             (sync/find-git-state (merge base-status
                                         {:missing #{"main/old-name.script"}
                                          :uncommited-changes #{"main/old-name.script"}
                                          :untracked #{"main/new-name.script"}})
                                  [{:change-type :delete
                                    :new-path nil
                                    :old-path "main/old-name.script"
                                    :score 0}
                                   {:change-type :add
                                    :new-path "main/new-name.script"
                                    :old-path nil
                                    :score 0}]))))
    (testing "Renamed and modified file, staged"
      (is (some? "This is covered by the `Renamed file, staged` case")))))

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

(deftest begin-flow-critical-failure-test
  (testing "Local changes are restored in case an error occurs inside begin-flow!"
    (with-git [git (new-git)
               prefs (make-prefs)
               added-path "/added.txt"
               added-contents "A file that has been added but not staged."
               existing-path "/existing.txt"
               existing-contents "A file that already existed in the repo, with unstaged changes."
               deleted-path "/deleted.txt"]
      ; Create some local changes.
      (create-file git existing-path "A file that already existed in the repo.")
      (create-file git deleted-path "A file that existed in the repo, but will be deleted.")
      (commit-src git)
      (create-file git existing-path existing-contents)
      (create-file git added-path added-contents)
      (delete-file git deleted-path)

      ; Create a directory where the flow journal file is expected to be
      ; in order to cause an Exception inside the begin-flow! function.
      ; Verify that the local changes remain afterwards.
      (let [journal-file (sync/flow-journal-file git)
            status-before (git/status git)]
        (.mkdirs journal-file)
        (is (thrown? java.io.IOException (sync/begin-flow! git prefs)))
        (io/delete-file (str (git/worktree git) "/.internal") :silently)
        (is (= status-before (git/status git)))
        (when (= status-before (git/status git))
          (is (= added-contents (slurp-file git added-path)))
          (is (= existing-contents (slurp-file git existing-path))))))))

(deftest resume-flow-test
  (with-git [git (new-git)]
    (create-file git "src/existing.txt" "A file that already existed in the repo.")
    (commit-src git)
    (create-file git "src/existing.txt" "A file that already existed in the repo, with unstaged changes.")
    (create-file git "src/added.txt" "A file that has been added but not staged.")
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

(deftest cancel-flow-from-partially-staged-rename-test
  (let [old-path "src/old-dir/file.txt"
        new-path "src/new-dir/file.txt"
        modified-path "src/modified.txt"
        setup-remote (fn []
                       (let [git (new-git)]
                         (create-file git ".gitignore" ".internal")
                         (create-file git old-path "A file that already existed in the repo, and will be renamed.")
                         (create-file git modified-path "A file that already existed in the repo.")
                         (-> git (.add) (.addFilepattern ".gitignore") (.call))
                         (-> git (.add) (.addFilepattern old-path) (.call))
                         (-> git (.add) (.addFilepattern modified-path) (.call))
                         (-> git (.commit) (.setMessage "message") (.call))
                         git))
        setup-local (fn [remote-git]
                      (let [git (clone remote-git)]
                        (create-file git modified-path "A file that already existed in the repo, with changes.")
                        (move-file git old-path new-path)
                        (let [{:keys [missing modified untracked]} (git/status git)]
                          (is (= #{modified-path} modified))
                          (is (= #{old-path} missing))
                          (is (= #{new-path} untracked)))
                        git))
        file-status (fn [git]
                      (select-keys (git/status git) [:added :changed :missing :modified :removed :untracked]))
        advance! (fn [!flow]
                   (swap! !flow sync/advance-flow progress/null-render-progress!))
        run-command! (fn [!flow command selection]
                       (test-util/handler-run command [{:name :sync :env {:selection selection :!flow !flow}}] {})
                       @!flow)
        perform-test! (fn [local-git staged-change unstaged-change]
                        (git/stage-change! local-git staged-change)
                        (let [status-before (file-status local-git)
                              !flow (sync/begin-flow! local-git (make-prefs))]
                          (is (= :pull/done (:state (advance! !flow))))
                          (is (= :push/start (:state (swap! !flow assoc :state :push/start))))
                          (let [{:keys [modified staged state]} (advance! !flow)]
                            (is (= :push/staging state))
                            (is (= #{(git/make-modify-change modified-path)
                                     unstaged-change} modified))
                            (is (= #{staged-change} staged)))
                          (let [{:keys [modified staged state]} (run-command! !flow :unstage-change [staged-change])]
                            (is (= :push/staging state))
                            (is (= #{(git/make-modify-change modified-path)
                                     (git/make-rename-change old-path new-path)} modified))
                            (is (= #{} staged)))
                          (sync/cancel-flow! !flow)
                          (is (= status-before (file-status local-git)))))]
    (testing "Renamed file, only stage add"
      (with-git [remote-git (setup-remote)
                 local-git (setup-local remote-git)
                 staged-change (git/make-add-change new-path)
                 unstaged-change (git/make-delete-change old-path)]
        (perform-test! local-git staged-change unstaged-change)))
    (testing "Renamed file, only stage delete"
      (with-git [remote-git (setup-remote)
                 local-git (setup-local remote-git)
                 staged-change (git/make-delete-change old-path)
                 unstaged-change (git/make-add-change new-path)]
        (perform-test! local-git staged-change unstaged-change)))))

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

(ns editor.sync-test
  (:require [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.dialogs :as dialogs]
            [editor.fs :as fs]
            [editor.git-test :refer :all]
            [editor.git :as git]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.sync :as sync]
            [integration.test-util :as test-util])
  (:import [java.io File]))

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
                     :conflicting-stage-state {}
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
             creds (git/credentials prefs)
             flow  @(sync/begin-flow! git creds)]
    (is (= :pull/start (:state flow)))
    (is (= git (:git flow)))
    (is (instance? org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider creds))
    (is (instance? org.eclipse.jgit.revwalk.RevCommit (:start-ref flow)))
    (is (nil? (:stash-info flow)))
    (is (true? (sync/flow-in-progress? git)))
    (is (flow-equal? flow @(sync/resume-flow git creds)))))

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
        (fs/create-directories! journal-file)
        (is (thrown? java.io.IOException (sync/begin-flow! git (git/credentials prefs))))
        (fs/delete-file! (File. (str (git/worktree git) "/.internal")) {:fail :silently})
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
          creds (git/credentials prefs)
          !flow (sync/begin-flow! git creds)]
      (is (flow-equal? @!flow @(sync/resume-flow git creds))))))

(deftest cancel-flow-in-progress-test
  (with-git [git (new-git)]
    (let [existing-contents "A file that already existed in the repo, with unstaged changes."
          added-contents "A file that has been added but not staged."]
      (create-file git "src/existing.txt" "A file that already existed in the repo.")
      (commit-src git)
      (create-file git "src/existing.txt" existing-contents)
      (create-file git "src/added.txt" added-contents)
      (let [status-before (simple-status git)]
        (sync/begin-flow! git (git/credentials (make-prefs)))
        (sync/cancel-flow-in-progress! git)
        (is (= status-before (simple-status git)))
        (is (= existing-contents (slurp-file git "src/existing.txt")))
        (is (= added-contents (slurp-file git "src/added.txt")))))))

(deftest cancel-flow-test
  (with-git [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (let [!flow (sync/begin-flow! git (git/credentials (make-prefs)))]
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
                              !flow (sync/begin-flow! local-git (git/credentials (make-prefs)))]
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

(deftest interactive-cancel-test
  (testing "Success"
    (let [cancel-fn (test-util/make-call-logger (constantly {:type :success}))]
      (is (nil? (sync/interactive-cancel! cancel-fn)))
      (is (= 1 (count (test-util/call-logger-calls cancel-fn))))))

  (testing "Error"
    (let [cancel-fn (test-util/make-call-logger (constantly {:type :error :can-retry? false}))]
      (with-redefs [dialogs/make-alert-dialog (test-util/make-call-logger)]
        (is (nil? (sync/interactive-cancel! cancel-fn)))
        (is (= 1 (count (test-util/call-logger-calls cancel-fn))))
        (is (= 1 (count (test-util/call-logger-calls dialogs/make-alert-dialog)))))))

  (testing "Retryable error"
    (testing "Don't retry"
      (let [cancel-fn (test-util/make-call-logger (constantly {:type :error :can-retry? true}))]
        (with-redefs [dialogs/make-confirm-dialog (test-util/make-call-logger (constantly false))]
          (is (nil? (sync/interactive-cancel! cancel-fn)))
          (is (= 1 (count (test-util/call-logger-calls cancel-fn))))
          (is (= 1 (count (test-util/call-logger-calls dialogs/make-confirm-dialog)))))))

    (testing "Retry"
      (let [cancel-fn (test-util/make-call-logger (constantly {:type :error :can-retry? true}))
            retry-responses (atom (list :unused true true false))
            answer-retry-query! (fn [_ _] (first (swap! retry-responses next)))]
        (with-redefs [dialogs/make-confirm-dialog (test-util/make-call-logger answer-retry-query!)]
          (is (nil? (sync/interactive-cancel! cancel-fn)))
          (is (= 3 (count (test-util/call-logger-calls cancel-fn))))
          (is (= 3 (count (test-util/call-logger-calls dialogs/make-confirm-dialog)))))))))

(defn- valid-error-message? [message]
  (and (string? message)
       (< 10 (count message))
       (not (string/blank? message))))

(defn- cancel-error-code-message [error-code]
  (#'sync/cancel-result-message {:code error-code}))

(defn- make-fake-ref-string []
  (string/join (repeatedly 40 #(rand-nth "0123456789abcdef"))))

(defn- setup-flow-in-progress! [git]
  (create-file git "src/existing.txt" "A file that already existed in the repo.")
  (commit-src git)
  (create-file git "src/existing.txt" "A file that already existed in the repo, with unstaged changes.")
  (let [!flow (sync/begin-flow! git (git/credentials (make-prefs)))]
    (is (true? (sync/flow-in-progress? git)))
    !flow))

(defn- update-flow-journal! [git update-fn]
  (let [file (sync/flow-journal-file git)
        old-data (sync/read-journal file)
        new-data (update-fn old-data)]
    (fs/create-file! file (pr-str new-data))))

(defn- perform-valid-flow-in-progress-cancel-tests [cancel-fn]
  (testing "success"
    (with-git [git (new-git)
               !flow (setup-flow-in-progress! git)]
      (let [result (cancel-fn !flow)]
        (is (= :success (:type result)))
        (is (false? (sync/flow-in-progress? git))))))

  (testing "locked-files-error"
    (with-git [git (new-git)
               !flow (setup-flow-in-progress! git)]
      (try
        (is (true? (.exists (git/file git "src/existing.txt"))))
        (lock-file git "src/existing.txt")
        (let [result (cancel-fn !flow)]
          (is (= :error (:type result)))
          (is (= :locked-files-error (:code result)))
          (is (valid-error-message? (cancel-error-code-message :locked-files-error)))
          (is (true? (:can-retry? result)))
          (is (true? (sync/flow-in-progress? git))))
        (finally
          (unlock-file git "src/existing.txt")))))

  (testing "revert-to-start-ref-error"
    (with-git [git (new-git)
               !flow (setup-flow-in-progress! git)]
      (with-redefs [git/revert-to-revision! (fn [_git _start-ref] (throw (java.io.IOException.)))]
        (let [result (cancel-fn !flow)]
          (is (= :error (:type result)))
          (is (= :revert-to-start-ref-error (:code result)))
          (is (valid-error-message? (cancel-error-code-message :revert-to-start-ref-error)))
          (is (true? (:can-retry? result)))
          (is (true? (sync/flow-in-progress? git)))))))

  (testing "stash-apply-error"
    (with-git [git (new-git)
               !flow (setup-flow-in-progress! git)]
      (with-redefs [git/stash-apply! (fn [_git _stash-info] (throw (java.io.IOException.)))]
        (let [result (cancel-fn !flow)]
          (is (= :error (:type result)))
          (is (= :stash-apply-error (:code result)))
          (is (valid-error-message? (cancel-error-code-message :stash-apply-error)))
          (is (true? (:can-retry? result)))
          (is (true? (sync/flow-in-progress? git)))))))

  (testing "stash-drop-error"
    (with-git [git (new-git)
               !flow (setup-flow-in-progress! git)]
      (with-redefs [git/stash-drop! (fn [_git _stash-info] (throw (java.io.IOException.)))]
        (let [result (cancel-fn !flow)]
          (is (= :warning (:type result)))
          (is (= :stash-drop-error (:code result)))
          (is (valid-error-message? (cancel-error-code-message :stash-drop-error)))
          (is (false? (:can-retry? result)))
          (is (false? (sync/flow-in-progress? git))))))))

(deftest cancel-flow-in-progress-errors-test
  (testing "deserialize-error"
    (with-git [git (new-git)]
      (setup-flow-in-progress! git)
      (update-flow-journal! git (constantly ["bad" "file" "format"]))
      (let [result (sync/cancel-flow-in-progress! git)]
        (is (= :error (:type result)))
        (is (= :deserialize-error (:code result)))
        (is (valid-error-message? (cancel-error-code-message :deserialize-error)))
        (is (false? (:can-retry? result)))
        (is (false? (sync/flow-in-progress? git))))))

  (testing "invalid-data-error"
    (with-git [git (new-git)]
      (setup-flow-in-progress! git)
      (update-flow-journal! git (constantly {:invalid-data "abc"}))
      (let [result (sync/cancel-flow-in-progress! git)]
        (is (= :error (:type result)))
        (is (= :invalid-data-error (:code result)))
        (is (valid-error-message? (cancel-error-code-message :invalid-data-error)))
        (is (false? (:can-retry? result)))
        (is (false? (sync/flow-in-progress? git))))))

  (testing "invalid-ref-error"
    (are [data]
      (with-git [git (new-git)]
        (setup-flow-in-progress! git)
        (update-flow-journal! git #(merge % data))
        (let [result (sync/cancel-flow-in-progress! git)]
          (is (= :error (:type result)))
          (is (= :invalid-ref-error (:code result)))
          (is (valid-error-message? (cancel-error-code-message :invalid-ref-error)))
          (is (false? (:can-retry? result)))
          (is (false? (sync/flow-in-progress? git)))))
      {:start-ref (make-fake-ref-string)}
      {:stash-info {:ref (make-fake-ref-string)}}
      {:start-ref (make-fake-ref-string) :stash-info {:ref (make-fake-ref-string)}}))

  (testing "read-error"
    (with-git [git (new-git)]
      (setup-flow-in-progress! git)
      (with-redefs [sync/read-journal (fn [_file] (throw (java.io.IOException.)))]
        (let [result (sync/cancel-flow-in-progress! git)]
          (is (= :error (:type result)))
          (is (= :read-error (:code result)))
          (is (valid-error-message? (cancel-error-code-message :read-error)))
          (is (false? (:can-retry? result)))
          (is (false? (sync/flow-in-progress? git)))))))

  (testing "valid-flow-in-progress-cancel-tests"
    (let [cancel-fn (fn [!flow] (sync/cancel-flow-in-progress! (:git @!flow)))]
      (perform-valid-flow-in-progress-cancel-tests cancel-fn))))

(deftest cancel-flow-errors-test
  (testing "valid-flow-in-progress-cancel-tests"
    (perform-valid-flow-in-progress-cancel-tests sync/cancel-flow!)))

(deftest finish-flow-test
  (with-git [git (new-git)]
    (let [!flow (sync/begin-flow! git (git/credentials (make-prefs)))]
      (is (true? (sync/flow-in-progress? git)))
      (sync/finish-flow! !flow)
      (is (false? (sync/flow-in-progress? git))))))

(defn- dotfile? [^File file]
  (= (subs (.getName file) 0 1) "."))

(defn- contents-by-file-path [^File root-dir]
  (let [visible-file? (fn [^File file] (and (.isFile file)
                                            (not (.isHidden file))
                                            (not (dotfile? file))))
        visible-directory? (fn [^File file] (and (.isDirectory file)
                                                 (not (.isHidden file))
                                                 (not (dotfile? file))))
        list-files (fn [^File directory] (.listFiles directory))
        file->pair (juxt (partial resource/relative-path root-dir) slurp)
        files (tree-seq visible-directory? list-files root-dir)]
    (into {} (comp (filter visible-file?) (map file->pair)) files)))

(defn- test-conflict-resolution [resolve! expected-status expected-contents-by-file-path]
  (with-git [remote-git (init-git)
             local-git (create-conflict-zoo! remote-git [".internal"])]
    (autostage remote-git)
    (-> remote-git .commit (.setMessage "Remote commit with conflicting changes") .call)
    (let [prefs (make-prefs)
          creds (git/credentials prefs)
          !flow (sync/begin-flow! local-git creds)
          advance! (fn [] (swap! !flow sync/advance-flow progress/null-render-progress!))
          resumable? (fn [] (is (flow-equal? @!flow @(sync/resume-flow local-git creds))))]
      (testing "Advances to conflicts stage"
        (is (= :pull/start (:state @!flow)))
        (advance!)
        (is (= :pull/conflicts (:state @!flow)))
        (is (resumable?)))

      (let [status-before (simple-status local-git)]
        (testing "Conflicts match expectations"
          (is (= (:conflicting-stage-state expected-status) (:conflicting-stage-state status-before))))

        (testing "Conflict resolution"
          (doseq [[file-path] (:conflicting-stage-state status-before)]
            (resolve! !flow file-path)
            (is (resumable?)))
          (is (= (:conflicting-stage-state expected-status) (:resolved @!flow)))
          (is (= #{} (:staged @!flow)))
          (advance!)
          (is (= :pull/done (:state @!flow)))
          (is (resumable?)))

        (testing "Project state after conflict resolution"
          (let [status-after (simple-status local-git)]
            (is (= (dissoc expected-status :conflicting-stage-state) status-after))
            (is (= expected-contents-by-file-path (contents-by-file-path (git/worktree local-git))))))))))

(deftest advance-flow-test
  (testing ":pull/done"
    (with-git [git       (new-git)
               local-git (clone git)
               prefs     (make-prefs)
               creds     (git/credentials prefs)
               !flow     (sync/begin-flow! local-git creds)]
      (is (flow-equal? @!flow @(sync/resume-flow local-git creds)))
      (swap! !flow sync/advance-flow progress/null-render-progress!)
      (is (= :pull/done (:state @!flow)))
      (is (flow-equal? @!flow @(sync/resume-flow local-git creds)))))

  (testing ":pull/conflicts"
    (testing "Use ours"
      (test-conflict-resolution sync/use-ours!
                                {:changed                #{"src/local_modified.txt"}
                                 :conflicting-stage-state {"src/both_modified_conflicting.txt"     :both-modified
                                                           "src/local_deleted_remote_modified.txt" :deleted-by-them
                                                           "src/local_modified_remote_deleted.txt" :deleted-by-us
                                                           "src/local_modified_remote_moved.txt"   :deleted-by-us
                                                           "src/local_moved_remote_modified.txt"   :deleted-by-them
                                                           "src/new/both_added_conflicting.txt"    :both-added}
                                 :missing                #{"src/local_deleted_remote_modified.txt"
                                                           "src/local_moved_remote_modified.txt"}
                                 :modified               #{"src/both_modified_conflicting.txt"
                                                           "src/new/both_added_conflicting.txt"}
                                 :removed                #{"src/local_deleted.txt"
                                                           "src/local_moved.txt"}
                                 :untracked              #{"src/local_modified_remote_deleted.txt"
                                                           "src/local_modified_remote_moved.txt"
                                                           "src/moved/both_moved_conflicting_a.txt"
                                                           "src/moved/local_moved.txt"
                                                           "src/moved/local_moved_remote_modified.txt"
                                                           "src/new/local_added.txt"}}
                                {"src/both_modified_conflicting.txt"         "both_modified_conflicting, modified by local."
                                 "src/both_modified_same.txt"                "both_modified_same, modified identically by both."
                                 "src/local_modified.txt"                    "local_modified, modified by local."
                                 "src/local_modified_remote_deleted.txt"     "local_modified_remote_deleted modified by local."
                                 "src/local_modified_remote_moved.txt"       "local_modified_remote_moved, modified by local."
                                 "src/moved/both_moved_conflicting_a.txt"    "both_moved_conflicting"
                                 "src/moved/both_moved_conflicting_b.txt"    "both_moved_conflicting"
                                 "src/moved/both_moved_same.txt"             "both_moved_same"
                                 "src/moved/local_modified_remote_moved.txt" "local_modified_remote_moved"
                                 "src/moved/local_moved.txt"                 "local_moved"
                                 "src/moved/local_moved_remote_modified.txt" "local_moved_remote_modified"
                                 "src/moved/remote_moved.txt"                "remote_moved"
                                 "src/new/both_added_conflicting.txt"        "both_added_conflicting, added by local."
                                 "src/new/both_added_same.txt"               "both_added_same, added identically by both."
                                 "src/new/local_added.txt"                   "local_added, added only by local."
                                 "src/new/remote_added.txt"                  "remote_added, added only by remote."
                                 "src/remote_modified.txt"                   "remote_modified, modified by remote."}))
    (testing "Use theirs"
      (test-conflict-resolution sync/use-theirs!
                                {:changed                #{"src/local_modified.txt"}
                                 :conflicting-stage-state {"src/both_modified_conflicting.txt"     :both-modified
                                                           "src/local_deleted_remote_modified.txt" :deleted-by-them
                                                           "src/local_modified_remote_deleted.txt" :deleted-by-us
                                                           "src/local_modified_remote_moved.txt"   :deleted-by-us
                                                           "src/local_moved_remote_modified.txt"   :deleted-by-them
                                                           "src/new/both_added_conflicting.txt"    :both-added}
                                 :removed                #{"src/local_deleted.txt"
                                                           "src/local_moved.txt"}
                                 :untracked              #{"src/moved/both_moved_conflicting_a.txt"
                                                           "src/moved/local_moved.txt"
                                                           "src/moved/local_moved_remote_modified.txt"
                                                           "src/new/local_added.txt"}}
                                {"src/both_modified_conflicting.txt"         "both_modified_conflicting, modified by remote."
                                 "src/both_modified_same.txt"                "both_modified_same, modified identically by both."
                                 "src/local_deleted_remote_modified.txt"     "local_deleted_remote_modified modified by remote."
                                 "src/local_modified.txt"                    "local_modified, modified by local."
                                 "src/local_moved_remote_modified.txt"       "local_moved_remote_modified modified by remote."
                                 "src/moved/both_moved_conflicting_a.txt"    "both_moved_conflicting"
                                 "src/moved/both_moved_conflicting_b.txt"    "both_moved_conflicting"
                                 "src/moved/both_moved_same.txt"             "both_moved_same"
                                 "src/moved/local_modified_remote_moved.txt" "local_modified_remote_moved"
                                 "src/moved/local_moved.txt"                 "local_moved"
                                 "src/moved/local_moved_remote_modified.txt" "local_moved_remote_modified"
                                 "src/moved/remote_moved.txt"                "remote_moved"
                                 "src/new/both_added_conflicting.txt"        "both_added_conflicting, added by remote."
                                 "src/new/both_added_same.txt"               "both_added_same, added identically by both."
                                 "src/new/local_added.txt"                   "local_added, added only by local."
                                 "src/new/remote_added.txt"                  "remote_added, added only by remote."
                                 "src/remote_modified.txt"                   "remote_modified, modified by remote."})))

  (testing ":push-staging -> :push/done"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (with-git [local-git (clone git)
                 prefs     (make-prefs)
                 creds     (git/credentials prefs)
                 !flow     (sync/begin-flow! local-git creds)]
        (swap! !flow assoc :state :push/start)
        (create-file local-git "/src/main.cpp" "void main() {BAR}")
        (swap! !flow sync/advance-flow progress/null-render-progress!)
        (is (= :push/staging (:state @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git creds)))
        (git/stage-change! local-git (git/make-modify-change "src/main.cpp"))
        (swap! !flow sync/refresh-git-state)
        (is (= #{(git/make-modify-change "src/main.cpp")} (:staged @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git creds)))
        (swap! !flow merge {:state   :push/committing
                            :message "foobar"})
        (is (flow-equal? @!flow @(sync/resume-flow local-git creds)))
        (swap! !flow sync/advance-flow progress/null-render-progress!)
        (is (= :push/done (:state @!flow)))
        (is (flow-equal? @!flow @(sync/resume-flow local-git creds)))))))

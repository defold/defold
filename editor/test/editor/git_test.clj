(ns editor.git-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.git :as git]
            [util.text-util :as text-util])
  (:import [java.io File]
           [org.eclipse.jgit.api Git]
           [org.eclipse.jgit.api.errors StashApplyFailureException]
           [org.eclipse.jgit.lib ObjectId]
           [org.eclipse.jgit.revwalk RevCommit]))

(defn create-dir
  ^File [git path]
  (let [f (git/file git path)]
    (fs/create-directories! f)))

(defn create-file
  ^File [git path content]
  (fs/create-file! (git/file git path) content))

(defn lock-file [git path]
  (let [f (git/file git path)]
    (.setReadOnly f)))

(defn unlock-file [git path]
  (let [f (git/file git path)]
    (.setWritable f true)))

(defn copy-file [git old-path new-path]
  (let [old-file (git/file git old-path)
        new-file (git/file git new-path)]
    (fs/copy-file! old-file new-file)))

(defn move-file [git old-path new-path]
  (let [old-file (git/file git old-path)
        new-file (git/file git new-path)]
    (fs/move-file! old-file new-file)))

(defn- temp-dir []
  (fs/create-temp-directory! "foo"))

(defn init-git []
  (-> (Git/init) (.setDirectory (temp-dir)) (.call)))

(defn new-git []
  (let [git (init-git)]
    ;; We must have a HEAD in general
    (create-file git "/dummy" "")
    (-> git (.add) (.addFilepattern "dummy") (.call))
    (-> git (.commit) (.setMessage "message") (.call))
    git))

(defn clone [git]
  (-> (Git/cloneRepository)
      (.setURI (.getAbsolutePath (.getWorkTree (.getRepository git))))
      (.setDirectory (io/file (temp-dir)))
      (.call)))

(defn delete-git [git]
  (let [work-tree (.getWorkTree (.getRepository git))]
    (.close git)
    (fs/delete-directory! work-tree)))

(defn delete-file [git file]
  (fs/delete-file! (git/file git file)))

(defn slurp-file [git file]
  (slurp (git/file git file)))

(defn- add-src [git]
  (-> git (.add) (.addFilepattern "src") (.call)))

(defn commit-src [git]
  (add-src git)
  (-> git (.commit) (.setMessage "message") (.call)))

(defn- all-files [status]
  (reduce (fn [acc [k v]] (clojure.set/union acc v)) #{} (dissoc status :untracked-folders)))

(defn autostage [^Git git]
  (let [s (git/status git)]
    (doseq [f (:modified s)]
      (-> git (.add) (.addFilepattern f) (.call)))

    (doseq [f (:untracked s)]
      (-> git (.add) (.addFilepattern f) (.call)))

    (doseq [f (:missing s)]
      (-> git (.rm) (.addFilepattern f) (.call)))))

(def ^:private assert-args #'clojure.core/assert-args)

(defmacro with-git
  "bindings => [name init ...]

  Helper for cleaning up temporary git repositories created during testing.
  Semantics and implementaiton are based on clojure.core/with-open.

  Evaluates body in a try expression with names bound to the values
  of the inits, and a finally clause that calls (delete-git name) on each
  name that was bount to a Git value in reverse order."
  [bindings & body]
  (assert-args
    (vector? bindings) "a vector for its binding"
    (even? (count bindings)) "an even number of forms in binding vector")
  (cond
    (= (count bindings) 0) `(do ~@body)
    (symbol? (bindings 0)) `(let ~(subvec bindings 0 2)
                              (try
                                (with-git ~(subvec bindings 2) ~@body)
                                (finally
                                  (when (instance? Git ~(bindings 0))
                                    (delete-git ~(bindings 0))))))
    :else (throw (IllegalArgumentException.
                   "with-git only allows Symbols in bindings"))))

;; ============================================================================================

(deftest autostage-test
  (with-git [git (new-git)]
    (testing "create files"
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/util.cpp" ""))

    (testing "Add and commit initial files"
      (commit-src git))

    (testing "Change util.cpp"
      (is (= #{} (:untracked (git/status git))))
      (create-file git "/src/util.cpp" "// stuff")
      (is (= #{"src/util.cpp"} (:modified (git/status git))))
      (is (= #{"src/util.cpp"} (:uncommitted-changes (git/status git))))
      (autostage git)
      (is (= #{} (:modified (git/status git))))
      (is (= #{"src/util.cpp"} (:uncommitted-changes (git/status git)))))

    (testing "Create new.cpp"
      (create-file git "/src/new.cpp" "")
      (is (= #{"src/new.cpp"} (:untracked (git/status git))))
      (autostage git)
      (is (= #{} (:untracked (git/status git)))))

    (testing "Delete main.cpp"
      (delete-file git "src/main.cpp")
      (is (= #{"src/main.cpp"} (:missing (git/status git))))
      (autostage git)
      (is (= #{} (:missing (git/status git))))
      (is (= #{"src/main.cpp"} (:removed (git/status git)))))))

(deftest worktree-test
  (with-git [git (new-git)]
    (is (instance? File (git/worktree git)))))

(deftest get-current-commit-ref-test
  (with-git [git (new-git)]
    (is (instance? RevCommit (git/get-current-commit-ref git)))))

(deftest status-test
  (with-git [git (new-git)]
    (testing "untracked"
      (create-file git "/src/main.cpp" "void main() {}")
      (is (= #{"src/main.cpp"} (:untracked (git/status git))))
      (is (= #{"src"} (:untracked-folders (git/status git)))))
    (testing "added"
      (add-src git)
      (is (= #{"src/main.cpp"} (:added (git/status git))))
      (is (= #{"src/main.cpp"} (:uncommitted-changes (git/status git)))))

    (testing "conflicts"
      (with-git [local-git (clone git)]
        (create-file git "/src/main.cpp" "void main() {BAR}")
        (create-file local-git "/src/main.cpp" "void main() {FOO}")
        (commit-src git)
        (commit-src local-git)
        (-> local-git .pull .call)
        (is (= #{"src/main.cpp"} (:conflicting (git/status local-git))))
        (is (= {"src/main.cpp" :both-added} (:conflicting-stage-state (git/status local-git))))))))

(deftest unified-status-test
  (testing "new file; unstaged and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (is (= [{:score 0, :change-type :add, :old-path nil, :new-path "src/main.cpp"}] (git/unified-status git)))
      (add-src git)
      (is (= [{:score 0, :change-type :add, :old-path nil, :new-path "src/main.cpp"}] (git/unified-status git)))))

  (testing "copied file; unstaged and staged"
    (with-git [git (new-git)
               expect [{:change-type :add
                        :new-path "src/after/copy.cpp"
                        :old-path nil
                        :score 0}
                       {:change-type :rename
                        :new-path "src/after/original.cpp"
                        :old-path "src/before/original.cpp"
                        :score 100}]]

      (create-file git "/src/before/original.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      ;; In order to create a scenario where JGit detects a copy, we need to
      ;; make a copy of a file below a directory and then rename the directory.
      (copy-file git "/src/before/original.cpp" "/src/before/copy.cpp")
      (move-file git "/src/before" "/src/after")
      (is (= expect (git/unified-status git)))
      (add-src git)
      (is (= expect (git/unified-status git)))))

  (testing "changed file; unstaged and staged and partially staged"
    (with-git [git (new-git)
               expect {:score 0, :change-type :modify, :old-path "src/main.cpp", :new-path "src/main.cpp"}]

      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/main.cpp" "void main2() {}")
      (is (= [expect] (git/unified-status git)))
      (add-src git)
      (is (= [expect] (git/unified-status git)))

      (create-file git "/src/main.cpp" "void main2() {}\nint x;")
      (add-src git)
      (is (= [expect] (git/unified-status git)))))

  (testing "delete and related"
    (with-git [git (new-git)]

      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      ;; stage delete for src/main.cpp
      (-> git (.rm) (.addFilepattern "src/main.cpp") (.call))
      (is (= [{:score 0, :change-type :delete, :old-path "src/main.cpp", :new-path nil}] (git/unified-status git)))

      ;; recreate src/main.cpp and expect in modified state
      (create-file git "/src/main.cpp" "void main2() {}")
      (is (= [{:score 0, :change-type :modify, :old-path "src/main.cpp", :new-path "src/main.cpp"}] (git/unified-status git)))))

  (testing "rename in work tree"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/foo.cpp" "void main() {}")
      (delete-file git "src/main.cpp")

      (is (= [{:score 100, :change-type :rename, :old-path "src/main.cpp", :new-path "src/foo.cpp"}] (git/unified-status git)))))

  (testing "rename deleted and staged file"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/foo.cpp" "void main() {}")
      (-> git (.rm) (.addFilepattern "src/main.cpp") (.call))

      (is (= [{:score 100, :change-type :rename, :old-path "src/main.cpp", :new-path "src/foo.cpp"}] (git/unified-status git)))))

  (testing "honor .gitignore"
    (with-git [git (new-git)
               expect {:score 0, :change-type :modify, :old-path "src/main.cpp", :new-path "src/main.cpp"}]

      (create-file git "/.gitignore" "*.cpp")
      (-> git (.add) (.addFilepattern ".gitignore") (.call))
      (-> git (.commit) (.setMessage "message") (.call))

      (is (= [] (git/unified-status git)))

      (create-file git "/src/main.cpp" "void main() {}")
      (is (= [] (git/unified-status git))))))

(deftest show-file-test
  (with-git [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (let [ref (commit-src git)]
      (is (nil? (git/show-file git "src/missing.cpp")))
      (is (nil? (git/show-file git "src/missing.cpp" (.name ref))))
      (is (= "void main() {}" (String. (git/show-file git "src/main.cpp"))))
      (is (= "void main() {}" (String. (git/show-file git "src/main.cpp" (.name ref))))))))

(deftest stage-unstage-added-file-test
  (with-git [git (new-git)]

    (create-file git "src/main.cpp" "void main() {}")
    (let [{:keys [added untracked]} (git/status git)]
      (is (= #{} added))
      (is (= #{"src/main.cpp"} untracked)))

    (git/stage-change! git (git/make-add-change "src/main.cpp"))
    (let [{:keys [added untracked]} (git/status git)]
      (is (= #{"src/main.cpp"} added))
      (is (= #{} untracked)))

    (git/unstage-change! git (git/make-add-change "src/main.cpp"))
    (let [{:keys [added untracked]} (git/status git)]
      (is (= #{} added))
      (is (= #{"src/main.cpp"} untracked)))))

(deftest stage-unstage-deleted-file-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (commit-src git)

    (delete-file git "src/main.cpp")
    (let [{:keys [removed missing]} (git/status git)]
      (is (= #{} removed))
      (is (= #{"src/main.cpp"} missing)))

    (git/stage-change! git (git/make-delete-change "src/main.cpp"))
    (let [{:keys [removed missing]} (git/status git)]
      (is (= #{"src/main.cpp"} removed))
      (is (= #{} missing)))

    (git/unstage-change! git (git/make-delete-change "src/main.cpp"))
    (let [{:keys [removed missing]} (git/status git)]
      (is (= #{} removed))
      (is (= #{"src/main.cpp"} missing)))))

(deftest stage-unstage-renamed-file-test
  (with-git [git (new-git)]
    (create-file git "src/old.cpp" "void main() {}")
    (commit-src git)

    (move-file git "src/old.cpp" "src/new.cpp")
    (let [{:keys [added removed missing untracked]} (git/status git)]
      (is (= #{} added))
      (is (= #{} removed))
      (is (= #{"src/old.cpp"} missing))
      (is (= #{"src/new.cpp"} untracked)))

    (git/stage-change! git (git/make-rename-change "src/old.cpp" "src/new.cpp"))
    (let [{:keys [added removed missing untracked]} (git/status git)]
      (is (= #{"src/new.cpp"} added))
      (is (= #{"src/old.cpp"} removed))
      (is (= #{} missing))
      (is (= #{} untracked)))

    (git/unstage-change! git (git/make-rename-change "src/old.cpp" "src/new.cpp"))
    (let [{:keys [added removed missing untracked]} (git/status git)]
      (is (= #{} added))
      (is (= #{} removed))
      (is (= #{"src/old.cpp"} missing))
      (is (= #{"src/new.cpp"} untracked)))))

(deftest stage-unstage-modified-file-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (commit-src git)

    (create-file git "src/main.cpp" "void main2() {}")
    (let [{:keys [modified changed]} (git/status git)]
      (is (= #{} changed))
      (is (= #{"src/main.cpp"} modified)))

    (git/stage-change! git (git/make-modify-change "src/main.cpp"))
    (let [{:keys [modified changed]} (git/status git)]
      (is (= #{"src/main.cpp"} changed))
      (is (= #{} modified)))

    (git/unstage-change! git (git/make-modify-change "src/main.cpp"))
    (let [{:keys [modified changed]} (git/status git)]
      (is (= #{} changed))
      (is (= #{"src/main.cpp"} modified)))))

(defn setup-modification-zoo [git]
  (create-file git "src/modified/file.txt" "A file that already existed in the repo. Will be modified, but not staged.")
  (create-file git "src/changed/file.txt" "A file that already existed in the repo. Will be modified, then staged.")
  (create-file git "src/missing/file.txt" "A file that already existed in the repo. Will be deleted, but not staged.")
  (create-file git "src/removed/file.txt" "A file that already existed in the repo. Will be deleted, then staged.")
  (create-file git "src/old-location-unstaged/file.txt" "A file that already existed in the repo. Will be moved, but not staged.")
  (create-file git "src/old-location-staged/file.txt" "A file that already existed in the repo. Will be moved, then staged.")
  (let [start-ref (commit-src git)]
    (create-dir git "src/empty-dir")
    (create-file git "src/modified/file.txt" "A file that already existed in the repo, with unstaged changes.")
    (create-file git "src/changed/file.txt" "A file that already existed in the repo, with staged changes.")
    (delete-file git "src/missing/file.txt")
    (delete-file git "src/removed/file.txt")
    (create-file git "src/untracked/file.txt" "A file that was added, but not staged.")
    (create-file git "src/added/file.txt" "A file that was added, then staged.")
    (move-file git "src/old-location-unstaged/file.txt" "src/new-location-unstaged/file.txt")
    (move-file git "src/old-location-staged/file.txt" "src/new-location-staged/file.txt")
    (git/stage-change! git (git/make-modify-change "src/changed/file.txt"))
    (git/stage-change! git (git/make-delete-change "src/removed/file.txt"))
    (git/stage-change! git (git/make-add-change "src/added/file.txt"))
    (git/stage-change! git (git/make-rename-change "src/old-location-staged/file.txt" "src/new-location-staged/file.txt"))
    (let [status-before (git/status git)]
      (is (= #{"src/added/file.txt" "src/new-location-staged/file.txt"} (:added status-before)))
      (is (= #{"src/changed/file.txt"} (:changed status-before)))
      (is (= #{"src/missing/file.txt" "src/old-location-unstaged/file.txt"} (:missing status-before)))
      (is (= #{"src/modified/file.txt"} (:modified status-before)))
      (is (= #{"src/removed/file.txt" "src/old-location-staged/file.txt"} (:removed status-before)))
      (is (= #{"src/untracked/file.txt" "src/new-location-unstaged/file.txt"} (:untracked status-before)))
      (is (= #{"src/untracked" "src/new-location-unstaged" "src/empty-dir"} (:untracked-folders status-before))))
    start-ref))

(defn simple-status
  "Calls git/status and filters out redundant info and empty collections."
  [git]
  (into {}
        (filter (fn [[k v]]
                  (and (seq v)
                       (not= k :conflicting) ; We keep :conflicting-stage-state instead.
                       (not= k :ignored-not-in-index)
                       (not= k :uncommitted-changes)
                       (not= k :untracked-folders))))
        (git/status git)))

(deftest revert-to-revision-test
  (with-git [git (new-git)
             start-ref (setup-modification-zoo git)]
    (git/revert-to-revision! git start-ref)
    (is (= {} (simple-status git)))))

(deftest stash-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (is (= {:untracked #{"src/main.cpp"}} (simple-status git)))
    (let [stash-info (git/stash! git)]
      (is (instance? RevCommit (:ref stash-info)))
      (is (= #{"src/main.cpp"} (:untracked stash-info))))
    (is (= {} (simple-status git)))))

(deftest stash-apply-test
  (with-git [git (new-git)]
    ; Create initial commit.
    (create-file git "src/staged/deleted.txt" "deleted")
    (create-file git "src/staged/modified.txt" "modified")
    (create-file git "src/staged/moved.txt" "moved")
    (create-file git "src/unstaged/deleted.txt" "deleted")
    (create-file git "src/unstaged/modified.txt" "modified")
    (create-file git "src/unstaged/moved.txt" "moved")
    (commit-src git)

    ; Stage some changes in the working directory.
    (delete-file git "src/staged/deleted.txt")
    (create-file git "src/staged/modified.txt" "modified...")
    (move-file git "src/staged/moved.txt" "src/staged/moved/moved.txt")
    (create-file git "src/staged/added.txt" "added")
    (autostage git)

    ; Make some unstaged changes in the working directory.
    (delete-file git "src/unstaged/deleted.txt")
    (create-file git "src/unstaged/modified.txt" "modified...")
    (move-file git "src/unstaged/moved.txt" "src/unstaged/moved/moved.txt")
    (create-file git "src/unstaged/added.txt" "added")

    ; Verify that the status is retained after stashing and applying the stash.
    (let [status-before (simple-status git)
          stash-info (git/stash! git)]
      (is (= {} (simple-status git)))
      (is (instance? ObjectId (git/stash-apply! git stash-info)))
      (let [status-after (simple-status git)]
        (is (= status-before status-after))
        (is (= {:added     #{"src/staged/added.txt"
                             "src/staged/moved/moved.txt"}
                :untracked #{"src/unstaged/added.txt"
                             "src/unstaged/moved/moved.txt"}
                :changed   #{"src/staged/modified.txt"}
                :modified  #{"src/unstaged/modified.txt"}
                :removed   #{"src/staged/deleted.txt"
                             "src/staged/moved.txt"}
                :missing   #{"src/unstaged/deleted.txt"
                             "src/unstaged/moved.txt"}} status-after))))))

(defn create-conflict-zoo!
  "Commit some files to the remote, then create a local clone.
  We will then make conflicting modifications to the working
  directories of both repositories"
  [^Git remote-git gitignore-patterns]
  (when-let [gitignore-contents (not-empty (string/join "\n" gitignore-patterns))]
    (create-file remote-git ".gitignore" gitignore-contents))
  (create-file remote-git "src/both_deleted.txt" "both_deleted")
  (create-file remote-git "src/both_modified_conflicting.txt" "both_modified_conflicting")
  (create-file remote-git "src/both_modified_same.txt" "both_modified_same")
  (create-file remote-git "src/both_moved_conflicting.txt" "both_moved_conflicting")
  (create-file remote-git "src/both_moved_same.txt" "both_moved_same")
  (create-file remote-git "src/local_deleted.txt" "local_deleted")
  (create-file remote-git "src/local_deleted_remote_modified.txt" "local_deleted_remote_modified")
  (create-file remote-git "src/local_modified.txt" "local_modified")
  (create-file remote-git "src/local_modified_remote_deleted.txt" "local_modified_remote_deleted")
  (create-file remote-git "src/local_modified_remote_moved.txt" "local_modified_remote_moved")
  (create-file remote-git "src/local_moved.txt" "local_moved")
  (create-file remote-git "src/local_moved_remote_modified.txt" "local_moved_remote_modified")
  (create-file remote-git "src/remote_deleted.txt" "remote_deleted")
  (create-file remote-git "src/remote_modified.txt" "remote_modified")
  (create-file remote-git "src/remote_moved.txt" "remote_moved")
  (-> remote-git .add (.addFilepattern ".") .call)
  (-> remote-git .commit (.setMessage "Initial commit") .call)
  (is (= {} (simple-status remote-git)))

  (let [local-git (clone remote-git)]
    (is (true? (.exists (git/file local-git "src/both_deleted.txt"))))
    (is (true? (.exists (git/file local-git "src/both_modified_conflicting.txt"))))
    (is (true? (.exists (git/file local-git "src/both_modified_same.txt"))))
    (is (true? (.exists (git/file local-git "src/both_moved_conflicting.txt"))))
    (is (true? (.exists (git/file local-git "src/both_moved_same.txt"))))
    (is (true? (.exists (git/file local-git "src/local_deleted.txt"))))
    (is (true? (.exists (git/file local-git "src/local_deleted_remote_modified.txt"))))
    (is (true? (.exists (git/file local-git "src/local_modified.txt"))))
    (is (true? (.exists (git/file local-git "src/local_modified_remote_deleted.txt"))))
    (is (true? (.exists (git/file local-git "src/local_modified_remote_moved.txt"))))
    (is (true? (.exists (git/file local-git "src/local_moved.txt"))))
    (is (true? (.exists (git/file local-git "src/local_moved_remote_modified.txt"))))
    (is (true? (.exists (git/file local-git "src/remote_deleted.txt"))))
    (is (true? (.exists (git/file local-git "src/remote_modified.txt"))))
    (is (true? (.exists (git/file local-git "src/remote_moved.txt"))))
    (is (= {} (simple-status local-git)))

    ; Both repositories are identical. Proceed to make conflicting
    ; modifications to their working directories.

    ; ---- Existing Files ----

    ; Both deleted.
    (delete-file local-git "src/both_deleted.txt")
    (delete-file remote-git "src/both_deleted.txt")

    ; Both modified, conflicting modification.
    (create-file local-git "src/both_modified_conflicting.txt" "both_modified_conflicting, modified by local.")
    (create-file remote-git "src/both_modified_conflicting.txt" "both_modified_conflicting, modified by remote.")

    ; Both modified, same modification.
    (create-file local-git "src/both_modified_same.txt" "both_modified_same, modified identically by both.")
    (create-file remote-git "src/both_modified_same.txt" "both_modified_same, modified identically by both.")

    ; Both moved, conflicting location.
    (move-file local-git "src/both_moved_conflicting.txt" "src/moved/both_moved_conflicting_a.txt")
    (move-file remote-git "src/both_moved_conflicting.txt" "src/moved/both_moved_conflicting_b.txt")

    ; Both moved, same location.
    (move-file local-git "src/both_moved_same.txt" "src/moved/both_moved_same.txt")
    (move-file remote-git "src/both_moved_same.txt" "src/moved/both_moved_same.txt")

    ; Local deleted.
    (delete-file local-git "src/local_deleted.txt")

    ; Local deleted, remote modified.
    (delete-file local-git "src/local_deleted_remote_modified.txt")
    (create-file remote-git "src/local_deleted_remote_modified.txt" "local_deleted_remote_modified modified by remote.")

    ; Local modified.
    (create-file local-git "src/local_modified.txt" "local_modified, modified by local.")

    ; Local modified, remote deleted.
    (create-file local-git "src/local_modified_remote_deleted.txt" "local_modified_remote_deleted modified by local.")
    (delete-file remote-git "src/local_modified_remote_deleted.txt")

    ; Local modifed, remote moved.
    (create-file local-git "src/local_modified_remote_moved.txt" "local_modified_remote_moved, modified by local.")
    (move-file remote-git "src/local_modified_remote_moved.txt" "src/moved/local_modified_remote_moved.txt")

    ; Local moved.
    (move-file local-git "src/local_moved.txt" "src/moved/local_moved.txt")

    ; Local moved, remote modified.
    (move-file local-git "src/local_moved_remote_modified.txt" "src/moved/local_moved_remote_modified.txt")
    (create-file remote-git "src/local_moved_remote_modified.txt" "local_moved_remote_modified modified by remote.")

    ; Remote deleted.
    (delete-file remote-git "src/remote_deleted.txt")

    ; Remote modified.
    (create-file remote-git "src/remote_modified.txt" "remote_modified, modified by remote.")

    ; Remote moved.
    (move-file remote-git "src/remote_moved.txt" "src/moved/remote_moved.txt")

    ; ---- New Files ----

    ; Both added, conflicting add.
    (create-file local-git "src/new/both_added_conflicting.txt" "both_added_conflicting, added by local.")
    (create-file remote-git "src/new/both_added_conflicting.txt" "both_added_conflicting, added by remote.")

    ; Both added, same add.
    (create-file local-git "src/new/both_added_same.txt" "both_added_same, added identically by both.")
    (create-file remote-git "src/new/both_added_same.txt" "both_added_same, added identically by both.")

    ; Local added.
    (create-file local-git "src/new/local_added.txt" "local_added, added only by local.")

    ; Remote added.
    (create-file remote-git "src/new/remote_added.txt" "remote_added, added only by remote.")

    (is (= {:untracked #{"src/moved/both_moved_conflicting_a.txt"
                         "src/moved/both_moved_same.txt"
                         "src/moved/local_moved.txt"
                         "src/moved/local_moved_remote_modified.txt"
                         "src/new/both_added_conflicting.txt"
                         "src/new/both_added_same.txt"
                         "src/new/local_added.txt"}
            :modified #{"src/both_modified_conflicting.txt"
                        "src/both_modified_same.txt"
                        "src/local_modified.txt"
                        "src/local_modified_remote_deleted.txt"
                        "src/local_modified_remote_moved.txt"}
            :missing #{"src/both_deleted.txt"
                       "src/both_moved_conflicting.txt"
                       "src/both_moved_same.txt"
                       "src/local_deleted.txt"
                       "src/local_deleted_remote_modified.txt"
                       "src/local_moved.txt"
                       "src/local_moved_remote_modified.txt"}} (simple-status local-git)))

    (is (= {:untracked #{"src/moved/both_moved_conflicting_b.txt"
                         "src/moved/both_moved_same.txt"
                         "src/moved/local_modified_remote_moved.txt"
                         "src/moved/remote_moved.txt"
                         "src/new/both_added_conflicting.txt"
                         "src/new/both_added_same.txt"
                         "src/new/remote_added.txt"}
            :modified #{"src/both_modified_conflicting.txt"
                        "src/both_modified_same.txt"
                        "src/local_deleted_remote_modified.txt"
                        "src/local_moved_remote_modified.txt"
                        "src/remote_modified.txt"}
            :missing #{"src/both_deleted.txt"
                       "src/both_moved_conflicting.txt"
                       "src/both_moved_same.txt"
                       "src/local_modified_remote_deleted.txt"
                       "src/local_modified_remote_moved.txt"
                       "src/remote_deleted.txt"
                       "src/remote_moved.txt"}} (simple-status remote-git)))

    local-git))

(deftest stash-apply-conflicting-test
  (with-git [remote-git (new-git)
             local-git (create-conflict-zoo! remote-git [])]
    ; Commit conflicting changes on remote.
    (autostage remote-git)
    (is (= {:added #{"src/moved/both_moved_conflicting_b.txt"
                     "src/moved/both_moved_same.txt"
                     "src/moved/local_modified_remote_moved.txt"
                     "src/moved/remote_moved.txt"
                     "src/new/both_added_conflicting.txt"
                     "src/new/both_added_same.txt"
                     "src/new/remote_added.txt"}
            :changed #{"src/both_modified_conflicting.txt"
                       "src/both_modified_same.txt"
                       "src/local_deleted_remote_modified.txt"
                       "src/local_moved_remote_modified.txt"
                       "src/remote_modified.txt"}
            :removed #{"src/both_deleted.txt"
                       "src/both_moved_conflicting.txt"
                       "src/both_moved_same.txt"
                       "src/local_modified_remote_deleted.txt"
                       "src/local_modified_remote_moved.txt"
                       "src/remote_deleted.txt"
                       "src/remote_moved.txt"}} (simple-status remote-git)))
    (-> remote-git .commit (.setMessage "Remote changes") .call)

    ; Stash conflicting changes on local.
    (is (= {:untracked #{"src/moved/both_moved_conflicting_a.txt"
                         "src/moved/both_moved_same.txt"
                         "src/moved/local_moved.txt"
                         "src/moved/local_moved_remote_modified.txt"
                         "src/new/both_added_conflicting.txt"
                         "src/new/both_added_same.txt"
                         "src/new/local_added.txt"}
            :modified #{"src/both_modified_conflicting.txt"
                        "src/both_modified_same.txt"
                        "src/local_modified.txt"
                        "src/local_modified_remote_deleted.txt"
                        "src/local_modified_remote_moved.txt"}
            :missing #{"src/both_deleted.txt"
                       "src/both_moved_conflicting.txt"
                       "src/both_moved_same.txt"
                       "src/local_deleted.txt"
                       "src/local_deleted_remote_modified.txt"
                       "src/local_moved.txt"
                       "src/local_moved_remote_modified.txt"}} (simple-status local-git)))
    (let [stash-info (git/stash! local-git)]
      ; Verify our working directory is clean.
      (is (= {} (simple-status local-git)))

      ; Pull from remote.
      (-> local-git .pull .call)

      ; Apply stashed changes.
      (is (thrown? StashApplyFailureException (git/stash-apply! local-git stash-info))))

    (testing "Files exist at new locations."
      (are [path] (true? (.exists (git/file local-git path)))
                  "src/moved/both_moved_conflicting_a.txt"
                  "src/moved/both_moved_conflicting_b.txt"
                  "src/moved/both_moved_same.txt"
                  "src/moved/local_modified_remote_moved.txt"
                  "src/moved/local_moved.txt"
                  "src/moved/local_moved_remote_modified.txt"
                  "src/moved/remote_moved.txt"
                  "src/new/both_added_conflicting.txt"
                  "src/new/both_added_same.txt"
                  "src/new/local_added.txt"
                  "src/new/remote_added.txt"))

    (testing "Files do not exist at old locations."
      (are [path] (false? (.exists (git/file local-git path)))
                  "src/both_deleted.txt"
                  "src/both_moved_conflicting.txt"
                  "src/both_moved_same.txt"
                  "src/local_deleted.txt"
                  "src/local_moved.txt"
                  "src/remote_deleted.txt"
                  "src/remote_moved.txt"))

    (testing "Modified files remain."
      (are [path] (true? (.exists (git/file local-git path)))
                  "src/both_modified_conflicting.txt"
                  "src/both_modified_same.txt"
                  "src/local_deleted_remote_modified.txt"
                  "src/local_modified.txt"
                  "src/local_modified_remote_deleted.txt"
                  "src/local_modified_remote_moved.txt"
                  "src/local_moved_remote_modified.txt"
                  "src/remote_modified.txt"))

    ; After the stash is applied, changes that were not staged at the time the
    ; stash was created will be staged in the working directory. I.e files that
    ; previously reported as :modified will now be reported as :changed, and
    ; :missing files will report as :removed. This is because we're in the
    ; middle of a conflicting merge. If there were no conflicts the staged
    ; state appears intact after a stash followed by a stash apply. This is
    ; verified by stash-apply-test above.
    ;
    ; The stash is being merged onto the latest commit from the remote, so in
    ; the :conflicting-stage-state map "us" is the remote and "them" is stash.
    (is (= {:untracked #{"src/moved/both_moved_conflicting_a.txt"
                         "src/moved/local_moved.txt"
                         "src/moved/local_moved_remote_modified.txt"
                         "src/new/local_added.txt"}
            :changed #{"src/local_modified.txt"}
            :conflicting-stage-state {"src/both_modified_conflicting.txt"     :both-modified
                                      "src/local_deleted_remote_modified.txt" :deleted-by-them
                                      "src/local_modified_remote_deleted.txt" :deleted-by-us
                                      "src/local_modified_remote_moved.txt"   :deleted-by-us
                                      "src/local_moved_remote_modified.txt"   :deleted-by-them
                                      "src/new/both_added_conflicting.txt"    :both-added}
            :removed #{"src/local_deleted.txt"
                       "src/local_moved.txt"}} (simple-status local-git)))))

(deftest stash-drop-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (let [status-before (simple-status git)
          stash-info (git/stash! git)]
      (git/stash-drop! git stash-info)
      (git/stash-apply! git stash-info)
      (testing "ref still is reflog"
        (is (= status-before (simple-status git)))))))

(deftest revert-test
  (testing "untracked"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (is (= #{"src/main.cpp"} (:untracked (git/status git))))
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "new and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")

      (autostage git)
      (is (= #{"src/main.cpp"} (:added (git/status git))))
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "changed"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (create-file git "/src/main.cpp" "void main2() {}")

      (is (= #{"src/main.cpp"} (:modified (git/status git))))
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "changed and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (create-file git "/src/main.cpp" "void main2() {}")

      (is (= #{"src/main.cpp"} (:modified (git/status git))))
      (autostage git)
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "deleted"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")

      (is (= #{"src/main.cpp" "src/other.cpp"} (:missing (git/status git))))
      (git/revert git ["src/other.cpp"])
      (is (= #{"src/main.cpp"} (:missing (git/status git))))))

  (testing "deleted and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")
      (autostage git)

      (is (= #{"src/main.cpp" "src/other.cpp"} (:removed (git/status git))))
      (git/revert git ["src/other.cpp"])
      (is (= #{"src/main.cpp"} (:removed (git/status git))))))

  (testing "renamed"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (create-file git "/src/foo.cpp" "void main() {}")
      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")

      (is (= #{"src/main.cpp" "src/other.cpp"} (:missing (git/status git))))
      (is (= #{"src/foo.cpp"} (:untracked (git/status git))))

      (git/revert git ["src/foo.cpp"])
      (is (= #{"src/other.cpp"} (all-files (git/status git))))))

  (testing "renamed and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (create-file git "/src/foo.cpp" "void main() {}")
      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")
      (autostage git)

      (is (= #{"src/main.cpp" "src/other.cpp"} (:removed (git/status git))))
      (is (= #{"src/foo.cpp"} (:added (git/status git))))

      (git/revert git ["src/foo.cpp"])
      (is (= #{"src/other.cpp"} (all-files (git/status git)))))))

(deftest locked-files-test
  (with-git [git (new-git)]
    (testing "Read-only files are considered locked."
      (let [a (create-file git "/src/a.txt" "file a")
            b (create-file git "/src/b.txt" "file b")]
        (try
          (is (= #{} (git/locked-files git)))
          (.setReadOnly a)
          (is (= #{a} (git/locked-files git)))
          (.setReadOnly b)
          (is (= #{a b} (git/locked-files git)))
          (.setWritable a true)
          (is (= #{b} (git/locked-files git)))
          (.setWritable b true)
          (is (= #{} (git/locked-files git)))
          (finally
            (.setWritable a true)
            (.setWritable b true)
            (fs/delete-file! a)
            (fs/delete-file! b)))))

    (testing "Excludes files below .git directory."
      (let [index (.getIndexFile (.getRepository git))]
        (try
          (.setReadOnly index)
          (is (= #{} (git/locked-files git)))
          (finally
            (.setWritable index true)))))

    (testing "Excludes ignored files and directories."
      (let [a (create-file git "/a.txt" "file a")
            b (create-file git "/b.txt" "file b")
            c (create-file git "/dir/c.txt" "file c")
            d (create-file git "/dir/d.txt" "file d")
            files #{a b c d}]
        (try
          (doseq [file files]
            (.setReadOnly file))
          (is (= files (git/locked-files git)))
          (create-file git ".gitignore" "a.txt\ndir/")
          (is (= #{b} (git/locked-files git)))
          (finally
            (doseq [file files]
              (.setWritable file true))))))))

(deftest ensure-gitignore-configured-test
  (testing "Default .gitignore contains required entries"
    (is (empty? (set/difference (set git/required-gitignore-entries)
                                (set git/default-gitignore-entries)))))

  (testing "Returns false for nil."
    (is (false? (git/ensure-gitignore-configured! nil))))

  (testing "No .gitignore file."
    (with-git [git (new-git)]
      (let [gitignore-file (git/file git ".gitignore")]
        (is (false? (.exists gitignore-file)))
        (is (true? (git/ensure-gitignore-configured! git)))
        (when (is (true? (.exists gitignore-file)))
          (is (= git/default-gitignore-entries
                 (string/split-lines (slurp gitignore-file))))))))

  (testing "Empty .gitignore file."
    (with-git [git (new-git)]
      (let [gitignore-file (git/file git ".gitignore")]
        (spit gitignore-file "\t    \n    \t\n")
        (is (true? (git/ensure-gitignore-configured! git)))
        (when (is (true? (.exists gitignore-file)))
          (is (= git/default-gitignore-entries
                 (string/split-lines (slurp gitignore-file))))))))

  (testing "Unconfigured .gitignore file."
    (with-git [git (new-git)]
      (let [gitignore-file (git/file git ".gitignore")
            gitignore-lines-before ["one.txt"
                                    "two.txt"]]
        (spit gitignore-file (string/join "\n" gitignore-lines-before))
        (is (true? (git/ensure-gitignore-configured! git)))
        (let [gitignore-lines-after (string/split-lines (slurp gitignore-file))]
          (is (= (into gitignore-lines-before git/required-gitignore-entries)
                 gitignore-lines-after))))))

  (testing "Partially configured .gitignore file."
    (with-git [git (new-git)]
      (let [gitignore-file (git/file git ".gitignore")
            gitignore-lines-before (vec (concat ["one.txt"
                                                 "two.txt"
                                                 "/.internal"
                                                 "three.txt"]))]
        (spit gitignore-file (string/join "\n" gitignore-lines-before))
        (is (true? (git/ensure-gitignore-configured! git)))
        (let [gitignore-lines-after (string/split-lines (slurp gitignore-file))]
          (is (= ["one.txt"
                  "two.txt"
                  "/.internal"
                  "three.txt"
                  "/build"]
                 gitignore-lines-after))))))

  (testing "Already-configured .gitignore file."
    (with-git [git (new-git)]
      (let [gitignore-file (git/file git ".gitignore")
            gitignore-lines-before (shuffle (concat git/required-gitignore-entries ["extra.txt"]))]
        (spit gitignore-file (string/join "\n" gitignore-lines-before))
        (is (false? (git/ensure-gitignore-configured! git)))
        (let [gitignore-lines-after (string/split-lines (slurp gitignore-file))]
          (is (= gitignore-lines-before gitignore-lines-after))))))

  (testing "Respects original line endings."
    (with-git [git (new-git)]
      (let [gitignore-file (git/file git ".gitignore")]
        (testing "CRLF"
          (spit gitignore-file "one.txt\r\ntwo.txt\r\n")
          (is (true? (git/ensure-gitignore-configured! git)))
          (is (= :crlf (text-util/scan-line-endings (io/reader gitignore-file)))))

        (testing "LF"
          (spit gitignore-file "one.txt\ntwo.txt\n")
          (is (true? (git/ensure-gitignore-configured! git)))
          (is (= :lf (text-util/scan-line-endings (io/reader gitignore-file))))))))

  (testing "Does not add patterns that are already matched."
    (with-git [git (new-git)]
      (let [gitignore-file (git/file git ".gitignore")
            gitignore-lines-before [".internal"
                                    "build"]]
        (is (= git/required-gitignore-entries (map fs/with-leading-slash gitignore-lines-before)))
        (spit gitignore-file (string/join "\n" gitignore-lines-before))
        (is (false? (git/ensure-gitignore-configured! git)))
        (let [gitignore-lines-after (string/split-lines (slurp gitignore-file))]
          (is (= gitignore-lines-before gitignore-lines-after)))))))

(deftest internal-files-are-tracked-test
  (testing "Returns false for nil."
    (is (false? (git/internal-files-are-tracked? nil))))

  (testing "No internal files."
    (with-git [git (new-git)]
      (is (false? (git/internal-files-are-tracked? git)))
      (create-file git "file.txt" "project file")
      (is (false? (git/internal-files-are-tracked? git)))
      (-> git (.add) (.addFilepattern "file.txt") (.call))
      (-> git (.commit) (.setMessage "Added project file") (.call))
      (is (false? (git/internal-files-are-tracked? git)))))

  (testing "Tracked internal files."
    (are [file-path]
      (with-git [git (new-git)]
        ;; Returns false if an internal file exists but was never committed.
        (create-file git file-path "internal file")
        (is (false? (git/internal-files-are-tracked? git)))

        ;; Returns true if an internal file was committed.
        (-> git (.add) (.addFilepattern file-path) (.call))
        (-> git (.commit) (.setMessage "Added internal file") (.call))
        (is (true? (git/internal-files-are-tracked? git)))

        ;; Returns true even though later commits do not include internal files.
        (create-file git "file.txt" "project file")
        (-> git (.add) (.addFilepattern "file.txt") (.call))
        (-> git (.commit) (.setMessage "Added project file") (.call))
        (is (true? (git/internal-files-are-tracked? git)))

        ;; Returns false after the internal file is deleted from the repository.
        (is (true? (.exists (git/file git file-path))))
        (-> git (.rm) (.addFilepattern file-path) (.call))
        (-> git (.commit) (.setMessage "Removed internal file") (.call))
        (is (false? (.exists (git/file git file-path))))
        (is (false? (git/internal-files-are-tracked? git))))

      ".internal/.sync-in-progress"
      "build/default/_generated_1234abcd.spritec")))

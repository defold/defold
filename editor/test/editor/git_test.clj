(ns editor.git-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.git :as git])
  (:import java.nio.file.attribute.FileAttribute
           java.nio.file.Files
           org.apache.commons.io.FileUtils
           [org.eclipse.jgit.api Git]))

(defn- git-file [^Git git path]
  (io/file (str (.getWorkTree (.getRepository git)) "/" path)))

(defn create-file [git path content]
  (let [f (git-file git path)]
    (io/make-parents f)
    (spit f content)))

(defn move-file [git old-path new-path]
  (let [old-file (git-file git old-path)
        new-file (git-file git new-path)]
    (io/make-parents new-file)
    (.renameTo old-file new-file)))

(defn- temp-dir []
  (.toFile (Files/createTempDirectory "foo" (into-array FileAttribute []))))

(defn new-git []
  (let [f (temp-dir)
        git (-> (Git/init) (.setDirectory f) (.call))]
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
  (FileUtils/deleteDirectory (.getWorkTree (.getRepository git))))

(defn delete-file [git file]
  (io/delete-file (git-file git file)))

(defn slurp-file [git file]
  (slurp (git-file git file)))

(defn- add-src [git]
  (-> git (.add) (.addFilepattern "src") (.call)))

(defn commit-src [git]
  (add-src git)
  (-> git (.commit) (.setMessage "message") (.call)))

(defn- all-files [status]
  (reduce (fn [acc [k v]] (clojure.set/union acc v)) #{} (dissoc status :untracked-folders)))

(defn- autostage [^Git git]
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
      (is (= #{"src/util.cpp"} (:uncommited-changes (git/status git))))
      (autostage git)
      (is (= #{} (:modified (git/status git))))
      (is (= #{"src/util.cpp"} (:uncommited-changes (git/status git)))))

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
    (is (instance? java.io.File (git/worktree git)))))

(deftest get-current-commit-ref-test
  (with-git [git (new-git)]
    (is (instance? org.eclipse.jgit.revwalk.RevCommit (git/get-current-commit-ref git)))))

(deftest status-test
  (with-git [git (new-git)]
    (testing "untracked"
      (create-file git "/src/main.cpp" "void main() {}")
      (is (= #{"/src/main.cpp" (:untracked (git/status git))}))
      (is (= #{"src" (:untracked-folders (git/status git))})))
    (testing "added"
      (add-src git)
      (is (= #{"/src/main.cpp" (:added (git/status git))}))
      (is (= #{"src" (:uncommnited-changes (git/status git))})))

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

(deftest hard-reset-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (let [ref (commit-src git)]
      (create-file git "src/main.cpp" "void main() {FOO}")
      (is (= #{"src/main.cpp"} (:modified (git/status git))))
      (git/hard-reset git ref)
      (is (= #{} (:modified (git/status git)))))))

(deftest stash-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (is (= #{"src/main.cpp"} (:untracked (git/status git))))
    (git/stash git)
    (is (= #{} (:untracked (git/status git))))))

(deftest stash-apply-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (is (= #{"src/main.cpp"} (:untracked (git/status git))))
    (let [ref (git/stash git)]
      (is (= #{} (:untracked (git/status git))))
      (git/stash-apply git ref)
      (is (= #{"src/main.cpp"} (:untracked (git/status git)))))))

(deftest stash-drop-test
  (with-git [git (new-git)]
    (create-file git "src/main.cpp" "void main() {}")
    (let [ref (git/stash git)]
      (git/stash-drop git ref)
      (git/stash-apply git ref)
      ;; ref still is reflog
      (is (= #{"src/main.cpp"} (:untracked (git/status git)))))))

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

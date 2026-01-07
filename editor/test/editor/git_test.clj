;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.git-test
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.fs :as fs]
            [editor.git :as git]
            [util.text-util :as text-util])
  (:import [ch.qos.logback.classic Level Logger]
           [java.io File]
           [org.eclipse.jgit.api Git]
           [org.eclipse.jgit.revwalk RevCommit]
           [org.slf4j LoggerFactory]))

(.setLevel ^Logger (LoggerFactory/getLogger "org.eclipse.jgit.util.FS") Level/ERROR)

(defn create-file
  ^File [git path content]
  (fs/create-file! (git/file git path) content))

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

(defn- add-src [git]
  (-> git (.add) (.addFilepattern "src") (.call)))

(defn commit-src [git]
  (add-src git)
  (-> git (.commit) (.setMessage "message") (.call)))

(defn- all-files [status]
  (reduce (fn [acc [k v]] (clojure.set/union acc v)) #{} (dissoc status :untracked-folders)))

(def ^:private assert-args #'clojure.core/assert-args)

(defmacro with-git
  "bindings => [name init ...]

  Helper for cleaning up temporary git repositories created during testing.
  Semantics and implementation are based on clojure.core/with-open.

  Evaluates body in a try expression with names bound to the values
  of the inits, and a finally clause that calls (delete-git name) on each
  name that was bound to a Git value in reverse order."
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
      (create-file git "/src/main.cpp" "void main() {}")
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

      (create-file git "/src/before/original.cpp" "void main() {}")
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

      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/main.cpp" "void main2() {}")
      (is (= [expect] (git/unified-status git)))
      (add-src git)
      (is (= [expect] (git/unified-status git)))

      (create-file git "/src/main.cpp" "void main2() {}\nint x;")
      (add-src git)
      (is (= [expect] (git/unified-status git)))))

  (testing "delete and related"
    (with-git [git (new-git)]

      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      ;; stage delete for src/main.cpp
      (-> git (.rm) (.addFilepattern "src/main.cpp") (.call))
      (is (= [{:score 0, :change-type :delete, :old-path "src/main.cpp", :new-path nil}] (git/unified-status git)))

      ;; recreate src/main.cpp and expect in modified state
      (create-file git "/src/main.cpp" "void main2() {}")
      (is (= [{:score 0, :change-type :modify, :old-path "src/main.cpp", :new-path "src/main.cpp"}] (git/unified-status git)))))

  (testing "rename in work tree"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/foo.cpp" "void main() {}")
      (delete-file git "src/main.cpp")

      (is (= [{:score 100, :change-type :rename, :old-path "src/main.cpp", :new-path "src/foo.cpp"}] (git/unified-status git)))))

  (testing "rename deleted and staged file"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/foo.cpp" "void main() {}")
      (-> git (.rm) (.addFilepattern "src/main.cpp") (.call))

      (is (= [{:score 100, :change-type :rename, :old-path "src/main.cpp", :new-path "src/foo.cpp"}] (git/unified-status git)))))

  (testing "honor .gitignore"
    (with-git [git (new-git)
               expect {:score 0, :change-type :modify, :old-path "src/main.cpp", :new-path "src/main.cpp"}]

      (create-file git "/.gitignore" "*.cpp")
      (-> git (.add) (.addFilepattern ".gitignore") (.call))
      (-> git (.commit) (.setMessage "message") (.call))

      (is (= [] (git/unified-status git)))

      (create-file git "/src/main.cpp" "void main() {}")
      (is (= [] (git/unified-status git))))))

(deftest show-file-test
  (with-git [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (let [ref (commit-src git)]
      (is (nil? (git/show-file git "src/missing.cpp")))
      (is (nil? (git/show-file git "src/missing.cpp" (.name ref))))
      (is (= "void main() {}" (String. (git/show-file git "src/main.cpp"))))
      (is (= "void main() {}" (String. (git/show-file git "src/main.cpp" (.name ref))))))))

(deftest revert-test
  (testing "untracked"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (is (= #{"src/main.cpp"} (:untracked (git/status git))))
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "new and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")

      (git/stage-all! git)
      (is (= #{"src/main.cpp"} (:added (git/status git))))
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "changed"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (create-file git "/src/main.cpp" "void main2() {}")

      (is (= #{"src/main.cpp"} (:modified (git/status git))))
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "changed and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (create-file git "/src/main.cpp" "void main2() {}")

      (is (= #{"src/main.cpp"} (:modified (git/status git))))
      (git/stage-all! git)
      (git/revert git ["src/main.cpp"])
      (is (= #{} (all-files (git/status git))))))

  (testing "deleted"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")

      (is (= #{"src/main.cpp" "src/other.cpp"} (:missing (git/status git))))
      (git/revert git ["src/other.cpp"])
      (is (= #{"src/main.cpp"} (:missing (git/status git))))))

  (testing "deleted and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")
      (git/stage-all! git)

      (is (= #{"src/main.cpp" "src/other.cpp"} (:removed (git/status git))))
      (git/revert git ["src/other.cpp"])
      (is (= #{"src/main.cpp"} (:removed (git/status git))))))

  (testing "renamed"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (create-file git "/src/foo.cpp" "void main() {}")
      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")

      (is (= #{"src/main.cpp" "src/other.cpp"} (:missing (git/status git))))
      (is (= #{"src/foo.cpp"} (:untracked (git/status git))))

      (git/revert git ["src/foo.cpp"])
      (is (= #{"src/other.cpp"} (all-files (git/status git))))))

  (testing "renamed and staged"
    (with-git [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (create-file git "/src/other.cpp" "void other() {}")
      (commit-src git)

      (create-file git "/src/foo.cpp" "void main() {}")
      (delete-file git "src/main.cpp")
      (delete-file git "src/other.cpp")
      (git/stage-all! git)

      (is (= #{"src/main.cpp" "src/other.cpp"} (:removed (git/status git))))
      (is (= #{"src/foo.cpp"} (:added (git/status git))))

      (git/revert git ["src/foo.cpp"])
      (is (= #{"src/other.cpp"} (all-files (git/status git))))))

  (testing "case-changed"
    (with-git [git (new-git)]
      (create-file git "src/main.cpp" "void main() {}")
      (create-file git "src/other.cpp" "void other() {}")
      (commit-src git)

      (move-file git "src/main.cpp" "src/MAIN.cpp")
      (delete-file git "src/other.cpp")

      (is (= #{"src/main.cpp" "src/other.cpp"} (:missing (git/status git))))
      (is (= #{"src/MAIN.cpp"} (:untracked (git/status git))))

      (git/revert git ["src/MAIN.cpp"])
      (is (= #{"src/other.cpp"} (all-files (git/status git))))))

  (testing "case-changed and staged"
    (with-git [git (new-git)]
      (create-file git "src/main.cpp" "void main() {}")
      (create-file git "src/other.cpp" "void other() {}")
      (commit-src git)

      (move-file git "src/main.cpp" "src/MAIN.cpp")
      (delete-file git "src/other.cpp")
      (git/stage-all! git)

      (is (= #{"src/main.cpp" "src/other.cpp"} (:removed (git/status git))))
      (is (= #{"src/MAIN.cpp"} (:added (git/status git))))

      (git/revert git ["src/MAIN.cpp"])
      (is (= #{"src/other.cpp"} (all-files (git/status git)))))))

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
                  "/.editor_settings"
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
            gitignore-lines-before [".editor_settings"
                                    ".internal"
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

(ns editor.git-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.git :as git])
  (:import [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]
           [org.eclipse.jgit.api Git]
           [org.apache.commons.io FileUtils]))

(defn- create-file [git path content]
  (let [f (io/file (str (.getWorkTree (.getRepository git)) "/" path))]
    (io/make-parents f)
    (spit f content)))

(defn- new-git []
  (let [f (.toFile (Files/createTempDirectory "foo" (into-array FileAttribute [])))
        git (-> (Git/init) (.setDirectory f) (.call))]
    ; We must have a HEAD in general
    (create-file git "/dummy" "")
    (-> git (.add) (.addFilepattern "dummy") (.call))
    (-> git (.commit) (.setMessage "message") (.call))
    git))

(defn- delete-git [git]
  (FileUtils/deleteDirectory (.getWorkTree (.getRepository git))))

(defn delete-file [git file]
  (io/delete-file (str (.getWorkTree (.getRepository git)) "/" file)))

(defn- add-src [git]
  (-> git (.add) (.addFilepattern "src") (.call)))

(defn- commit-src [git]
  (add-src git)
  (-> git (.commit) (.setMessage "message") (.call)))

(defn- all-files [status]
  (reduce (fn [acc [k v]] (clojure.set/union acc v)) #{} (dissoc status :untracked-folders)))

(deftest autostage-test
  (let [git (new-git)]
    ; Create files
    (create-file git "/src/main.cpp" "void main() {}")
    (create-file git "/src/util.cpp" "")

    ; Add and commit initial files
    (commit-src git)

    ; Change util.cpp
    (is (= #{} (:untracked (git/status git))))
    (create-file git "/src/util.cpp" "// stuff")
    (is (= #{"src/util.cpp"} (:modified (git/status git))))
    (is (= #{"src/util.cpp"} (:uncommited-changes (git/status git))))
    (git/autostage git)
    (is (= #{} (:modified (git/status git))))
    (is (= #{"src/util.cpp"} (:uncommited-changes (git/status git))))

    ; Create new.cpp
    (create-file git "/src/new.cpp" "")
    (is (= #{"src/new.cpp"} (:untracked (git/status git))))
    (git/autostage git)
    (is (= #{} (:untracked (git/status git))))

    ; Delete main.cpp
    (delete-file git "src/main.cpp")
    (is (= #{"src/main.cpp"} (:missing (git/status git))))
    (git/autostage git)
    (is (= #{} (:missing (git/status git))))
    (is (= #{"src/main.cpp"} (:removed (git/status git))))

    (delete-git git)))

(deftest simple-status-test
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (is (= {"src/main.cpp" :added} (git/simple-status git)))
    (git/autostage git)
    (is (= {"src/main.cpp" :added} (git/simple-status git)))
    (delete-git git))

  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (is (= #{} (all-files (git/status git))))

    (delete-file git "src/main.cpp")
    (is (= {"src/main.cpp" :deleted} (git/simple-status git)))
    (git/autostage git)
    (is (= {"src/main.cpp" :deleted} (git/simple-status git)))
    (delete-git git))

  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (create-file git "/src/main.cpp" "void main2() {}")

    (is (= {"src/main.cpp" :modified} (git/simple-status git)))
    (git/autostage git)
    (is (= {"src/main.cpp" :modified} (git/simple-status git)))
    (delete-git git)))


(deftest unified-status-test
  ; new file
  ; unstaged and staged
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (is (= [{:score 0, :change-type :add, :old-path nil, :new-path "src/main.cpp"}] (git/unified-status git)))
    (add-src git)
    (is (= [{:score 0, :change-type :add, :old-path nil, :new-path "src/main.cpp"}] (git/unified-status git)))
    (delete-git git))

  ; changed file
  ; unstaged and staged and partially staged
  (let [git (new-git)
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
    (is (= [expect] (git/unified-status git)))

    (delete-git git))

  ; delete and related
  (let [git (new-git)]

    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (is (= [] (git/unified-status git)))

    ; stage delete for src/main.cpp
    (-> git (.rm) (.addFilepattern "src/main.cpp") (.call))
    (is (= [{:score 0, :change-type :delete, :old-path "src/main.cpp", :new-path nil}] (git/unified-status git)))

    ; recreate src/main.cpp and expect in modified state
    (create-file git "/src/main.cpp" "void main2() {}")
    (is (= [{:score 0, :change-type :modify, :old-path "src/main.cpp", :new-path "src/main.cpp"}] (git/unified-status git)))

    (delete-git git))

  ; rename in work tree
  (let [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/foo.cpp" "void main() {}")
      (delete-file git "src/main.cpp")

      (is (= [{:score 100, :change-type :rename, :old-path "src/main.cpp", :new-path "src/foo.cpp"}] (git/unified-status git)))

      (delete-git git))

  ; rename deleted and staged file
  (let [git (new-git)]
      (create-file git "/src/main.cpp" "void main() {}")
      (commit-src git)
      (is (= [] (git/unified-status git)))

      (create-file git "/src/foo.cpp" "void main() {}")
      (-> git (.rm) (.addFilepattern "src/main.cpp") (.call))

      (is (= [{:score 100, :change-type :rename, :old-path "src/main.cpp", :new-path "src/foo.cpp"}] (git/unified-status git)))

      (delete-git git)))

(deftest revert-test
  ; untracked
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (is (= #{"src/main.cpp"} (:untracked (git/status git))))
    (git/revert git ["src/main.cpp"])
    (is (= #{} (all-files (git/status git))))
    (delete-git git))

  ; new and staged
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")

    (git/autostage git)
    (is (= #{"src/main.cpp"} (:added (git/status git))))
    (git/revert git ["src/main.cpp"])
    (is (= #{} (all-files (git/status git))))
    (delete-git git))

  ; changed
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (create-file git "/src/main.cpp" "void main2() {}")

    (is (= #{"src/main.cpp"} (:modified (git/status git))))
    (git/revert git ["src/main.cpp"])
    (is (= #{} (all-files (git/status git))))
    (delete-git git))

  ; changed and staged
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    (create-file git "/src/main.cpp" "void main2() {}")

    (is (= #{"src/main.cpp"} (:modified (git/status git))))
    (git/autostage git)
    (git/revert git ["src/main.cpp"])
    (is (= #{} (all-files (git/status git))))
    (delete-git git))

  ; renamed and staged
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)

    (create-file git "/src/foo.cpp" "void main() {}")
    (delete-file git "src/main.cpp")

    (is (= #{"src/main.cpp"} (:missing (git/status git))))
    (is (= #{"src/foo.cpp"} (:untracked (git/status git))))
    ;(git/autostage git)
    (git/revert git ["src/foo.cpp"])
    (is (= #{} (all-files (git/status git))))
    (delete-git git))

  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)

    (create-file git "/src/foo.cpp" "void main() {}")
    (delete-file git "src/main.cpp")

    (is (= #{"src/main.cpp"} (:missing (git/status git))))
    (is (= #{"src/foo.cpp"} (:untracked (git/status git))))
    (git/autostage git)
    (git/revert git ["src/foo.cpp"])
    (is (= #{} (all-files (git/status git))))
    (delete-git git)))


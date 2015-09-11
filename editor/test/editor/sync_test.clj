(ns editor.sync-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.git :as git]
            [editor.sync :as sync])
  (:import [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]
           [org.eclipse.jgit.api Git]
           [org.apache.commons.io FileUtils]))

(defn- ws-file [git path]
  (let [f (io/file (str (.getWorkTree (.getRepository git)) "/" path))]
    f))

(defn- create-file [git path content]
  (let [f (ws-file git path)]
    (io/make-parents f)
    (spit f content)))

(defn slurp-file [git path]
  (slurp (ws-file git path)))

(defn- temp-dir []
  (.toFile (Files/createTempDirectory "foo" (into-array FileAttribute []))))

(defn- new-git []
  (let [f (temp-dir)
        git (-> (Git/init) (.setDirectory f) (.call))]
    ; We must have a HEAD in general
    (create-file git "/dummy" "")
    (-> git (.add) (.addFilepattern "dummy") (.call))
    (-> git (.commit) (.setMessage "initial") (.call))
    git))

(defn- add-src [git]
  (-> git (.add) (.addFilepattern "src") (.call)))

(defn- commit-src [git]
  (add-src git)
  (-> git (.commit) (.setMessage "message") (.call)))

(defn- clone [git]
  (-> (Git/cloneRepository)
      (.setURI (.getAbsolutePath (.getWorkTree (.getRepository git))))
      (.setDirectory (io/file (temp-dir)))
      (.call)))

(defn- setup-repos []
  (let [git (new-git)]
    (create-file git "/src/main.cpp" "void main() {}")
    (commit-src git)
    [(clone git) git]))

(defn- all-files [status]
  (reduce (fn [acc [k v]] (clojure.set/union acc v)) #{} (dissoc status :untracked-folders)))

(deftest unified-status-test)

(deftest sync-test
  (testing "cancel"
    (let [[git remote] (setup-repos)]
      (create-file git "foobar" "hej")
      (let [flow (sync/make-flow git "test")]
        (sync/cancel-flow flow)
        (is (= "master" (git/branch-name git)))
        (is (= #{"foobar"} (:untracked (git/status git))))
        (is (= "hej" (slurp-file git "foobar"))))))

  (testing "new file"
    (let [[git remote] (setup-repos)]
      (create-file git "foobar" "hej")
      (let [flow (sync/make-flow git "test")
            flow (sync/push-flow flow ["foobar"])]
        (is (= :success (:status flow)))
        (is (= #{} (:untracked (git/status git))))
        (is (= "hej" (slurp-file git "foobar"))))))

  (testing "alter file"
    (let [[git remote] (setup-repos)]
      (create-file git "/src/main.cpp" "void main2() {}")
      (let [flow (sync/make-flow git "test")
            flow (sync/push-flow flow ["src/main.cpp"])]
        (is (= :success (:status flow)))
        (is (= #{} (:untracked (git/status git))))
        (is (= "void main2() {}" (slurp-file git "/src/main.cpp"))))))

  (testing "uncommited conflicts"
    (let [[git remote] (setup-repos)]
      (create-file git "/src/main.cpp" "void main1() {}")
      (create-file remote "/src/main.cpp" "void main2() {}")
      (commit-src remote)
      (let [flow (sync/make-flow git "test")
            flow (sync/push-flow flow [])
            status (git/status git)]
        (is (= :checkout-conflict (:status flow)))
        (is (= "void main1() {}" (slurp-file git "/src/main.cpp")))
        (is (= #{"src/main.cpp"} (:modified (git/status git)))))))

  (testing "both modified"
    (doseq [resolution [:ours :theirs]]
      (let [[git remote] (setup-repos)]
        (create-file git "/src/main.cpp" "void main1() {}")
        (create-file remote "/src/main.cpp" "void main2() {}")
        (commit-src remote)
        (let [flow (sync/make-flow git "test")
              flow (sync/push-flow flow ["src/main.cpp"])
              status (git/status git)]
          (is (= {"src/main.cpp" :both-modified} (:conflicting-stage-state status)))
          (is (= :merging (:status flow)))
          (is (= #{} (:untracked (git/status git))))
          (is (= #{"src/main.cpp"} (:conflicting (git/status git))))
          (sync/resolve-file flow "src/main.cpp" resolution)
          (sync/commit-merge flow)
          (is (= #{} (all-files (git/status git))))
          (if (= resolution :ours)
            (is (= "void main1() {}" (slurp-file git "src/main.cpp")))
            (is (= "void main2() {}" (slurp-file git "src/main.cpp"))))))))

  (testing "deleted by us"
    (doseq [resolution [:ours :theirs]]
      (let [[git remote] (setup-repos)]
       (io/delete-file (ws-file git "/src/main.cpp"))
       (create-file remote "/src/main.cpp" "void main2() {}")
       (commit-src remote)
       (let [flow (sync/make-flow git "test")
             flow (sync/push-flow flow ["src/main.cpp"])
             status (git/status git)]
         (is (= {"src/main.cpp" :deleted-by-us} (:conflicting-stage-state status)))
         (is (= :merging (:status flow)))
         (is (= #{"src/main.cpp"} (:conflicting (git/status git))))
         (sync/resolve-file flow "src/main.cpp" resolution)
         (sync/commit-merge flow)
         (is (= #{} (all-files (git/status git))))
         (if (= resolution :ours)
           (is (= false (.exists (ws-file git "src/main.cpp"))))
           (is (= "void main2() {}" (slurp-file git "src/main.cpp"))))))))

  (testing "deleted by them"
    (doseq [resolution [:ours :theirs]]
      (let [[git remote] (setup-repos)]
        (-> (.rm remote) (.addFilepattern "src/main.cpp") (.call))
        (commit-src remote)
        (create-file git "/src/main.cpp" "void main1() {}")
        (let [flow (sync/make-flow git "test")
              flow (sync/push-flow flow ["src/main.cpp"])
              status (git/status git)]
          (is (= {"src/main.cpp" :deleted-by-them} (:conflicting-stage-state status)))
          (is (= :merging (:status flow)))
          (is (= #{"src/main.cpp"} (:conflicting (git/status git))))
          (sync/resolve-file flow "src/main.cpp" resolution)
          (sync/commit-merge flow)
          (is (= #{} (all-files (git/status git))))
          (if (= resolution :ours)
            (is (= "void main1() {}" (slurp-file git "src/main.cpp")))
            (is (= false (.exists (ws-file git "src/main.cpp")))))))))

  (testing "both added"
    (doseq [resolution [:ours :theirs]]
      (let [[git remote] (setup-repos)]
        (create-file git "/src/new.cpp" "a")
        (create-file remote "/src/new.cpp" "b")
        (commit-src remote)
        (let [flow (sync/make-flow git "test")
              flow (sync/push-flow flow ["src/new.cpp"])
              status (git/status git)]
          (is (= {"src/new.cpp" :both-added} (:conflicting-stage-state status)))
          (is (= :merging (:status flow)))
          (is (= #{"src/new.cpp"} (:conflicting (git/status git))))
          (sync/resolve-file flow "src/new.cpp" resolution)
          (sync/commit-merge flow)
          (is (= #{} (all-files (git/status git))))
          (if (= resolution :ours)
            (is (= "a" (slurp-file git "src/new.cpp")))
            (is (= "b" (slurp-file git "src/new.cpp")))))))))

(run-tests)

#_(let [git (Git/open (io/file "/var/folders/3c/v90zsd9x5bzfbbddqp5ym6x40000gr/T/foo3289790038167270088")) ]
  (-> (.status git)
      .call
      .getConflictingStageState))

#_(let [git (Git/open (io/file "/var/folders/3c/v90zsd9x5bzfbbddqp5ym6x40000gr/T/foo3289790038167270088")) ]
  (-> (.commit git)
      .call))

;

#_(let [git (new-git)]
  (create-file git "foobar" "hej")
  (let [flow (sync/make-flow git "test")]
    flow
    #_(-> git .getRepository .getBranch)))

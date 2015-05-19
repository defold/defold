(ns editor.git
"
Status API notes:
There's there different functions in this namespace

(status) \"Raw\" api very similar to \"git status\", index and work tree, but without rename detection support
(simple-status) Simplifcation of status where work tree and index are merged
(unified-status) Status based on actual content on disk and does not take index into account. Primary purpose is for the changes view. Rename detection is supported.

"
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [editor.prefs :as prefs]
            [editor.ui :as ui])
  (:import [javafx.scene.control ProgressBar]
           [org.eclipse.jgit.api Git]
           [org.eclipse.jgit.diff DiffEntry RenameDetector]
           [org.eclipse.jgit.revwalk RevCommit RevWalk]
           [org.eclipse.jgit.treewalk TreeWalk FileTreeIterator]
           [org.eclipse.jgit.lib BatchingProgressMonitor Repository]
           [org.eclipse.jgit.treewalk.filter PathFilter]
           [org.eclipse.jgit.transport UsernamePasswordCredentialsProvider]))

(set! *warn-on-reflection* true)

(defn show-file [^Git git name]
  (let [repo (.getRepository git)
        lastCommitId (.resolve repo "HEAD")
        rw (RevWalk. repo)
        commit (.parseCommit rw lastCommitId)
        tree (.getTree commit)
        tw (TreeWalk. repo)]
    (.addTree tw tree)
    (.setRecursive tw true)
    (.setFilter tw (PathFilter/create name))
    (.next tw)
    (let [id (.getObjectId tw 0)
          loader (.open repo id)
          ret (.getBytes loader)]
      (.dispose rw)
      (.close repo)
      ret)))

(defn- get-commit [^Repository repository revision]
  (let [walk (RevWalk. repository)]
    (.setRetainBody walk true)
    (.parseCommit walk (.resolve repository revision))))

(defn- diff-entry->map [^DiffEntry de]
  ; NOTE: We convert /dev/null to nil.
  (let [f (fn [p] (when-not (= p "/dev/null") p))]
    {:score (.getScore de)
     :change-type (-> (.getChangeType de) str .toLowerCase keyword)
     :old-path (f (.getOldPath de))
     :new-path (f (.getNewPath de))}))

(defn unified-status [^Git git]
  "Get the actual status by comparing contents on disk and HEAD. The index, i.e. staged files, are ignored."
  (let [repository (.getRepository git)
        tw (TreeWalk. repository)]
    (.setRecursive tw true)
    (.addTree tw (.getTree ^RevCommit (get-commit repository "HEAD")))
    (.addTree tw (FileTreeIterator. repository))
    (let [rd (RenameDetector. repository)]
      (.addAll rd (DiffEntry/scan tw))
      (mapv diff-entry->map (.compute rd (.getObjectReader tw) nil)))))

(defn status [^Git git]
  (let [s (-> git (.status) (.call))]
    {:added (.getAdded s)
     :changed (.getChanged s)
     :conflicting (.getConflicting s)
     :ignored-not-in-index (.getIgnoredNotInIndex s)
     :missing (.getMissing s)
     :modified (.getModified s)
     :removed (.getRemoved s)
     :uncommited-changes (.getUncommittedChanges s)
     :untracked (.getUntracked s)
     :untracked-folders (.getUntrackedFolders s)}))

(defn simple-status [^Git git]
  (let [s (status git)
        added (set/union (:added s) (:untracked s))
        deleted (set/union (:removed s) (:missing s))
        modified (set/union (:changed s) (:modified s))
        to-map (fn [s c] (zipmap s (repeat c)))]
    (-> {}
      (merge (to-map added :added))
      (merge (to-map deleted :deleted))
      (merge (to-map modified :modified)))))

; "High level" revert
; * Changed files are checked out
; * New files are removed
; * Deleted files are checked out
; NOTE: Not to be confused with "git revert"
(defn revert [^Git git files]
  (let [co (-> git (.checkout))
        reset (-> git (.reset) (.setRef "HEAD"))]
    (doseq [f files]
      (.addPath co f)
      (.addPath reset f))
    (.call reset)
    (.call co))

  ; Delete all untracked files in "files" as a last step
  ; JGit doesn't support this operation with RmCommand
  (let [s (status git)]
    (doseq [f files]
      (when (contains? (:untracked s) f)
        (io/delete-file (str (.getWorkTree (.getRepository git)) "/" f))))))

(defn autostage [^Git git]
  (let [s (status git)]
    (doseq [f (:modified s)]
     (-> git (.add) (.addFilepattern f) (.call)))

    (doseq [f (:untracked s)]
      (-> git (.add) (.addFilepattern f) (.call)))

    (doseq [f (:missing s)]
     (-> git (.rm) (.addFilepattern f) (.call)))))

(defn credentials [prefs]
  (let [email (prefs/get-prefs prefs "email" nil)
        token (prefs/get-prefs prefs "token" nil)]
    (UsernamePasswordCredentialsProvider. ^String email ^String token)))

(defn make-clone-monitor [^ProgressBar progress-bar]
  (let [tasks (atom {"remote: Finding sources" 0
                     "remote: Getting sizes" 0
                     "remote: Compressing objects" 0
                     "Receiving objects" 0
                     "Resolving deltas" 0})
        set-progress (fn [task percent] (swap! tasks assoc task percent))
        current-progress (fn [] (let [n (count @tasks)]
                                  (if (zero? n) 0 (/ (reduce + (vals @tasks)) (float n) 100.0))))]
    (proxy [BatchingProgressMonitor] []
     (onUpdate
       ([taskName workCurr])
       ([taskName workCurr workTotal percentDone]
         (set-progress taskName percentDone)
         (ui/run-later (.setProgress progress-bar (current-progress)))))

     (onEndTask
       ([taskName workCurr])
       ([taskName workCurr workTotal percentDone])))))

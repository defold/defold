(ns editor.git
  (:require [editor.prefs :as prefs]
            [editor.ui :as ui])
  (:import [javafx.scene.control ProgressBar]
           [org.eclipse.jgit.lib BatchingProgressMonitor]
           [org.eclipse.jgit.transport UsernamePasswordCredentialsProvider]))

(set! *warn-on-reflection* true)

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

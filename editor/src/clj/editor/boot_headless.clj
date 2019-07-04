(ns editor.boot-headless
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.bundle-android :as bundle-android]
            [editor.defold-project :as project]
            [editor.resource-types :as resource-types]
            [editor.workspace :as workspace]
            [internal.system :as is])
  (:import [java.io File]
           [javafx.application Platform]))

;;(set! *warn-on-reflection* true)

(defn- setup-system
  []
  (is/make-system {:cache-size 1000}))

(defn- setup-workspace
  [system ^File project-file]
  (let [world (first (keys (is/graphs system)))
        workspace (workspace/make-workspace world (.getCanonicalPath (.getParentFile project-file)) {})]
    (resource-types/register-resource-types! workspace)
    (println "Syncing resources")
    (workspace/resource-sync! workspace)
    (println "Sync done.")
    workspace))

(defn- setup-project
  [workspace]
  (let [proj-graph (g/make-graph! :history true :volatility 1)
        project (project/make-project proj-graph workspace)
        project (project/load-project project)]
    (g/reset-undo! proj-graph)
    project))

(defn -main
  [& args]
  (println "Welcome to the test bundling of Raket!")
  (println)
  (let [game-project-file (io/file (first args) "game.project")
        game-project-file-path (.getAbsolutePath game-project-file)
        output-dir (io/file (second args))]
    (try
      (cond
        (< (count args) 2)
        (println "Usage:\ndefold bundle <project directory> <output directory> [platform]")

        (not (.exists game-project-file))
        (println "Could not find" game-project-file-path)

        (not (.exists output-dir))
        (println "Could not find" (.getAbsolutePath output-dir))

        :else
        (do
          (println "Initializing system.")
          (let [system (setup-system)]
            (binding [g/*the-system* (atom system)]
              (println "Creating workspace.")
              (let [workspace (setup-workspace system game-project-file)
                    _ (println "Creating project.")
                    project (setup-project workspace)]
                (println "Project created.")
                (println "Bundling Raket! for Android.")
                (bundle-android/bundle! project output-dir {})
                (println "Bundling done, enjoy your game!")
                )))))
      (finally
        ;; Shut down any background stuff that may be keeping the process alive.
        (try
          (Platform/exit)
          (finally
            (shutdown-agents)))))))



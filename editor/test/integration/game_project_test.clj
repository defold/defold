(ns integration.game-project-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [integration.test-util :as test-util]
            [editor.game-project :as gp]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [service.log :as log])
  (:import [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [org.apache.commons.io FilenameUtils FileUtils]))

(def ^:dynamic ^String *project-path*)

(defn- create-test-project []
  (alter-var-root #'*project-path* (fn [_] (-> (Files/createTempDirectory "foo" (into-array FileAttribute []))
                                             (.toFile)
                                             (.getAbsolutePath))))
  (FileUtils/copyDirectory (File. "resources/reload_project") (File. *project-path*)))

(defn- load-test-project [ws-graph]
  (let [workspace (test-util/setup-workspace! ws-graph *project-path*)
        project (test-util/setup-project! workspace)]
    [workspace project]))

(defn- setup [ws-graph]
  (create-test-project)
  (load-test-project ws-graph))

(defn- mkdirs [^File f]
  (let [parent (.getParentFile f)]
    (when (not (.exists parent))
      (.mkdirs parent))))

(defn- ^File file-in-project
  [name]
  (File. (File. *project-path*) ^String name))

(defn- write-file [^String name content]
  (let [f (file-in-project name)]
    (mkdirs f)
    (if (not (.exists f))
      (.createNewFile f)
      (Thread/sleep 1100))
    (spit f content)))

(defn- copy-file [name new-name]
  (FileUtils/copyFile
   (file-in-project name)
   (file-in-project new-name)))

(defn- error? [type v]
  (and (g/error? v) (= type (get-in v [:user-data :type]))))

(defn- no-error? [v]
  (not (g/error? v)))

(defn- title [settings]
  (settings ["project" "title"]))

(deftest load-ok-project
  (with-clean-system
    (let [[workspace project] (setup world)]
      (testing "Settings loaded"
        (let [settings (g/node-value project :settings)]
          (is (= "Side-scroller" (title settings))))))))

(deftest load-broken-project
  (with-clean-system
    (create-test-project)
    (write-file "game.project" "bad content")
    (let [[workspace project] (log/without-logging (load-test-project world))]
      (testing "Defaults if can't load"
        (let [settings (g/node-value project :settings)]
          (is (= "unnamed" (title settings)))))
      (testing "Game project node is defective"
        (let [gpn (project/get-resource-node project "/game.project")
              gpn-settings-map (g/node-value gpn :settings-map)]
          (is (error? :invalid-content gpn-settings-map)))))))

(deftest break-ok-project
  (with-clean-system
    (let [[workspace project] (setup world)]
      (copy-file "game.project" "game.project.backup")
      (testing "Settings loaded"
        (let [settings (g/node-value project :settings)]
          (is (= "Side-scroller" (title settings)))))
      (testing "Broken file gives defaults & defective node"
        (write-file "game.project" "bad content")
        (log/without-logging (workspace/resource-sync! workspace))
        (let [settings (g/node-value project :settings)
              gpn (project/get-resource-node project "/game.project")
              gpn-settings-map (g/node-value gpn :settings-map)]
          (is (= "unnamed" (title settings)))
          (is (error? :invalid-content gpn-settings-map)))
        (copy-file "game.project.backup" "game.project")
        (workspace/resource-sync! workspace))
      (testing "Restoring gives normal settings"
        (let [settings (g/node-value project :settings)
              gpn (project/get-resource-node project "/game.project")
              gpn-settings-map (g/node-value gpn :settings-map)]
          (is (= "Side-scroller" (title settings)))
          (is (no-error? gpn-settings-map)))))))

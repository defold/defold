(ns internal.system
  (:require [clojure.core.async :as a :refer [<! chan go-loop put! close! onto-chan chan sliding-buffer]]
            [com.stuartsierra.component :as component]
            [dynamo.project :as p]
            [internal.graph.dgraph :as dg])
  (:import [org.eclipse.core.resources IProject]))

(defprotocol ProjectLifecycle
  (open-project [this project]  "Attach to the project and set up any internal state required.")
  (close-project [this] "Dispose any resources held by the project and release it."))

(defrecord ProjectSubsystem [project-state tx-report-queue]
  component/Lifecycle
  (start [this] this)

  (stop [this]
    (if project-state
      (close-project this)
      this))

  ProjectLifecycle
  (open-project [this eclipse-project]
    (when project-state
      (close-project this))
    (assoc this :project-state (ref (assoc (p/make-project tx-report-queue) :eclipse-project eclipse-project))))

  (close-project [this]
    (when project-state
      (p/dispose-project project-state)
      (dissoc this :project-state))))

(defn- project-subsystem
  [tx-report-queue]
  (ProjectSubsystem. nil tx-report-queue))

(defrecord BackgroundProcessor [starter queue control-chan]
  component/Lifecycle
   (start [this]
    (if control-chan
      this
      (assoc this :control-chan (starter queue))))

  (stop [this]
    (if control-chan
      (do
        (close! control-chan)
        (dissoc this :control-chan)))))

(defn- disposal-messages
  [tx-report]
  (for [v (:values-to-dispose tx-report)]
    {:project (:project-state tx-report)
     :value v}))

(defn disposal-loop
  [in]
  (go-loop []
           (when-let [v (<! in)]
             (.dispose (:value v))
             (recur))))

(defn- disposal-subsystem
  [disposal-queue]
  (BackgroundProcessor. disposal-loop disposal-queue nil))

(defn- refresh-messages
  [tx-report]
  (for [[node output] (:expired-outputs)]
    {:project (:project-state tx-report)
     :node    node
     :output  output}))

(defn- refresh-loop
  [in]
  (go-loop []
           (when-let [{:keys [project node output]} (<! in)]
             (p/get-resource-value project (dg/node (:graph project) node) output)
             (recur))))

(defn- refresh-subsystem
  [refresh-queue]
  (BackgroundProcessor. refresh-loop refresh-queue nil))

(defrecord Editor [started]
  component/Lifecycle
  (start [this]
    (if started
      this
      (assoc this :started true)))
  (stop [this]
    (if started
      (dissoc this :started)
      this)))

(defn- editor
  []
  (Editor. false))

(defn shred-tx-reports
  [in]
  (let [dispose (chan (sliding-buffer 1000))
        refresh (chan (sliding-buffer 1000))]
    (go-loop []
             (let [tx-report (<! in)]
               (if tx-report
                 (do
                   (onto-chan dispose (disposal-messages tx-report) false)
                   (onto-chan refresh (refresh-messages tx-report) false)
                   (recur))
                 (do
                   (close! dispose)
                   (close! refresh)))))
    {:dispose dispose :refresh refresh}))

(defn system
 []
 (let [tx-report-chan (chan 1)
       {:keys [dispose refresh]} (shred-tx-reports tx-report-chan)]
   (component/map->SystemMap
     {:project  (project-subsystem tx-report-chan)
      :disposal (disposal-subsystem dispose)
      :refresh  (refresh-subsystem  refresh)
      :editor   (component/using (editor) [:project :disposal :refresh])})))

(def ^:private the-system (atom (system)))

(defn start
  []
  (when-not (:started @the-system)
    (swap! the-system component/start)))

(defn stop
  []
  (when (:started @the-system)
    (swap! the-system component/stop)))

(defn attach-project
  [project]
  (swap! the-system update-in [:project] open-project project))

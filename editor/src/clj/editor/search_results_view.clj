(ns editor.search-results-view
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project-search :as project-search]
            [editor.dialogs :as dialogs]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [java.util Collection List]
           [javafx.animation AnimationTimer]
           [javafx.event Event]
           [javafx.scene Parent Scene]
           [javafx.scene.control SelectionMode TabPane TextField TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout AnchorPane]
           [javafx.stage StageStyle]))

(set! *warn-on-reflection* true)

(defrecord MatchContextResource [parent-resource line caret-position match]
  resource/Resource
  (children [_this]      [])
  (ext [_this]           (resource/ext parent-resource))
  (resource-name [_this] (format "%d: %s" (inc line) match))
  (resource-type [_this] (resource/resource-type parent-resource))
  (source-type [_this]   (resource/source-type parent-resource))
  (exists? [_this]       (resource/exists? parent-resource))
  (read-only? [_this]    (resource/read-only? parent-resource))
  (path [_this]          (resource/path parent-resource))
  (abs-path [_this]      (resource/abs-path parent-resource))
  (proj-path [_this]     (resource/proj-path parent-resource))
  (workspace [_this]     (resource/workspace parent-resource))
  (resource-hash [_this] (resource/resource-hash parent-resource))
  (openable? [_this]     (resource/openable? parent-resource)))

(defn- make-match-tree-item [resource {:keys [line caret-position match]}]
  (TreeItem. (->MatchContextResource resource line caret-position match)))

(defn- insert-search-result [^TreeView tree-view search-result]
  (let [{:keys [resource matches]} search-result
        match-items (map (partial make-match-tree-item resource) matches)
        resource-item (TreeItem. resource)]
    (.setExpanded resource-item true)
    (-> resource-item .getChildren (.addAll ^Collection match-items))
    (let [^List tree-items (.getChildren (.getRoot tree-view))
          first-match? (.isEmpty tree-items)]
      (.add tree-items resource-item)
      (when first-match?
        (-> tree-view .getSelectionModel (.clearAndSelect 1))))))

(defn- seconds->nanoseconds [seconds]
  (long (* 1000000000 seconds)))

(defn- start-tree-update-timer! [tree-views poll-fn]
  (let [timer (ui/->timer "tree-update-timer"
                          (fn [^AnimationTimer timer _]
                            (let [start-time (System/nanoTime)
                                  end-time (+ start-time (seconds->nanoseconds (/ 1 90)))]
                              (loop [poll-time 0]
                                (when (> end-time poll-time)
                                  (when-let [entry (poll-fn)]
                                    (if (= ::project-search/done entry)
                                      (.stop timer)
                                      (do (doseq [^TreeView tree-view tree-views]
                                            (insert-search-result tree-view entry))
                                          (recur (System/nanoTime))))))))))]
    (doseq [^TreeView tree-view tree-views]
      (.clear (.getChildren (.getRoot tree-view))))
    (ui/timer-start! timer)
    timer))

(defn- init-search-in-files-tree-view! [^TreeView tree-view]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (.setShowRoot tree-view false)
  (.setRoot tree-view (doto (TreeItem.)
                        (.setExpanded true)))
  (ui/cell-factory! tree-view
                    (fn [r]
                      (if (instance? MatchContextResource r)
                        {:text (resource/resource-name r)
                         :icon (workspace/resource-icon r)
                         :style (resource/style-classes r)}
                        {:text (resource/proj-path r)
                         :icon (workspace/resource-icon r)
                         :style (resource/style-classes r)}))))

(defn- make-search-in-files-tree-view
  ^TreeView []
  (doto (TreeView.) init-search-in-files-tree-view!))

(defn- resolve-search-in-files-tree-view-selection [selection]
  (into []
        (comp (filter resource/exists?)
              (keep (fn [resource]
                      (cond
                        (instance? MatchContextResource resource)
                        [(:parent-resource resource) {:caret-position (:caret-position resource)
                                                      :line (inc (:line resource))}]

                        :else
                        [resource {}]))))
        selection))

(def ^:private search-in-files-dialog-state (atom nil))

(defn- start-search-in-files! [project results-tab-tree-view open-fn show-find-results-fn]
  (let [root      ^Parent (ui/load-fxml "search-in-files-dialog.fxml")
        scene     (Scene. root)
        stage     (doto (ui/make-stage)
                    (.initStyle StageStyle/DECORATED)
                    (.initOwner (ui/main-stage))
                    (.setResizable false))]
    (ui/with-controls root [^TextField search ^TextField types ^TreeView resources-tree ok]
      (let [start-consumer! (partial start-tree-update-timer! [resources-tree results-tab-tree-view])
            stop-consumer! ui/timer-stop!
            report-error! (fn [error] (ui/run-later (throw error)))
            file-resource-save-data-future (project-search/make-file-resource-save-data-future report-error! project)
            {:keys [abort-search! start-search!]} (project-search/make-file-searcher file-resource-save-data-future start-consumer! stop-consumer! report-error!)
            on-input-changed! (fn [_ _ _]
                                (let [term (.getText search)
                                      exts (.getText types)]
                                  (reset! search-in-files-dialog-state {:term term :exts exts})
                                  (start-search! term exts)))
            dismiss-and-abort-search! (fn []
                                        (abort-search!)
                                        (ui/close! stage))
            dismiss-and-show-find-results! (fn []
                                             (show-find-results-fn)
                                             (ui/close! stage))
            open-selected! (fn []
                             (doseq [[resource opts] (resolve-search-in-files-tree-view-selection (ui/selection resources-tree))]
                               (open-fn resource opts)))]
        (ui/title! stage "Search in Files")
        (init-search-in-files-tree-view! resources-tree)

        (ui/on-action! ok (fn on-ok! [_] (dismiss-and-show-find-results!)))
        (ui/on-double! resources-tree (fn on-double! [^Event event]
                                        (.consume event)
                                        (open-selected!)))

        (.addEventFilter scene KeyEvent/KEY_PRESSED
                         (ui/event-handler event
                           (let [^KeyEvent event event]
                             (condp = (.getCode event)
                               KeyCode/DOWN   (when (and (= TextField (type (.getFocusOwner scene)))
                                                         (not= 0 (.getExpandedItemCount resources-tree)))
                                                (.consume event)
                                                (ui/request-focus! resources-tree))
                               KeyCode/ESCAPE (do (.consume event)
                                                  (dismiss-and-abort-search!))
                               KeyCode/ENTER  (do (.consume event)
                                                  (if (= TextField (type (.getFocusOwner scene)))
                                                    (dismiss-and-show-find-results!)
                                                    (open-selected!)))
                               KeyCode/TAB    (when (and (= types (.getFocusOwner scene))
                                                         (= 0 (.getExpandedItemCount resources-tree)))
                                                (.consume event)
                                                (ui/request-focus! search))
                               nil))))

        (let [{:keys [term exts]} @search-in-files-dialog-state]
          (ui/text! search term)
          (ui/text! types exts)
          (start-search! term exts))

        (ui/observe (.textProperty search) on-input-changed!)
        (ui/observe (.textProperty types) on-input-changed!)
        (.setScene stage scene)
        (ui/show! stage)))))

(g/defnode SearchResultsView
  (inherits core/Scope)
  (property open-resource-fn g/Any)
  (property search-results-container g/Any))

(defn- open-resource! [search-results-view resource opts]
  (let [open-resource-fn (g/node-value search-results-view :open-resource-fn)]
    (open-resource-fn resource opts)))

(defn- update-search-results! [search-results-view ^TreeView results-tree-view]
  (let [search-results-container ^AnchorPane (g/node-value search-results-view :search-results-container)
        open-selected! (fn []
                         (doseq [[resource opts] (resolve-search-in-files-tree-view-selection (ui/selection results-tree-view))]
                           (open-resource! search-results-view resource opts)))]
    ;; Replace tree view.
    (doto (.getChildren search-results-container)
      (.clear)
      (.add (doto results-tree-view
              (ui/fill-control)
              (ui/on-double! (fn on-double! [^Event event]
                               (.consume event)
                               (open-selected!))))))

    ;; Select tab-pane.
    (let [^TabPane tab-pane (.getParent (.getParent search-results-container))]
      (.select (.getSelectionModel tab-pane) 3))))

(defn make-search-results-view! [view-graph ^AnchorPane search-results-container open-resource-fn]
  (let [view-id (g/make-node! view-graph SearchResultsView
                              :open-resource-fn open-resource-fn
                              :search-results-container search-results-container)]
    view-id))

(defn show-search-in-files-dialog! [search-results-view project]
  (let [results-tab-tree-view (make-search-in-files-tree-view)
        open-fn (partial open-resource! search-results-view)
        show-matches-fn (partial update-search-results! search-results-view results-tab-tree-view)]
    (start-search-in-files! project results-tab-tree-view open-fn show-matches-fn)))

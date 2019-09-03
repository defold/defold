(ns editor.search-results-view
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.core :as core]
            [editor.defold-project-search :as project-search]
            [editor.jfx :as jfx]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [java.util Collection]
           [javafx.animation AnimationTimer]
           [javafx.event Event]
           [javafx.geometry Pos]
           [javafx.scene Parent Scene]
           [javafx.scene.control CheckBox Label ProgressIndicator SelectionMode TextField TreeItem TreeView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.scene.layout AnchorPane HBox Priority]
           [javafx.stage StageStyle]))

(set! *warn-on-reflection* true)

(defn- make-match-tree-item [resource match]
  (TreeItem. (assoc match :resource resource)))

(defn- make-result-tree-item [search-result]
  (let [{:keys [resource matches]} search-result
        match-items (map (partial make-match-tree-item resource) matches)
        resource-item (TreeItem. resource)]
    (.setExpanded resource-item true)
    (.addAll (.getChildren resource-item) ^Collection match-items)
    resource-item))

(defn- results->search-results+done? [results]
  (loop [[result & more] results
         tree-items (transient [])]
    (cond
      (nil? result)
      [(persistent! tree-items) false]

      (= ::project-search/done result)
      [(persistent! tree-items) true]

      :else
      (recur more (conj! tree-items result)))))

(defn- start-tree-update-timer! [tree-views progress-indicators results-fn]
  (let [timer (ui/->timer 5 "tree-update-timer"
                          (fn [^AnimationTimer timer elapsed-time]
                            ;; Delay showing the progress indicator a bit to avoid flashing for
                            ;; searches that complete quickly.
                            (when (< 0.2 elapsed-time)
                              (doseq [node progress-indicators]
                                (ui/visible! node true)))

                            (let [[search-results done?] (results->search-results+done? (results-fn))]
                              (when done?
                                (.stop timer)
                                (doseq [node progress-indicators]
                                  (ui/visible! node false)))
                              (when-not (empty? search-results)
                                (doseq [^TreeView tree-view tree-views
                                        :let [tree-children (.getChildren (.getRoot tree-view))
                                              first-match? (.isEmpty tree-children)
                                              focus-model (.getFocusModel tree-view)
                                              ;; focus model slows adding children drastically
                                              ;; if there is a focused item
                                              ;; removing focus before adding and then
                                              ;; restoring it results in same behavior
                                              ;; with much better performance
                                              focused-index (.getFocusedIndex focus-model)]]
                                  (.focus focus-model -1)
                                  (.addAll tree-children ^Collection (mapv make-result-tree-item search-results))
                                  (.focus focus-model focused-index)
                                  (when first-match?
                                    (.clearAndSelect (.getSelectionModel tree-view) 1)))))))]
    (doseq [^TreeView tree-view tree-views]
      (.clear (.getChildren (.getRoot tree-view))))
    (ui/timer-start! timer)
    timer))

(defn- make-matched-item-icon
  [{:keys [resource] :as _item}]
  (-> resource
      workspace/resource-icon
      (jfx/get-image-view 16)))

(defn- make-matched-item-row-indicator
  ^Label [{:keys [^long row] :as _item}]
  (doto (Label. (str (inc row) ": "))
    (ui/add-style! "line-number")
    (.setMinWidth Label/USE_PREF_SIZE)))

(defn- make-matched-item-before-text
  ^Label [{:keys [^String line ^long start-col] :as _item}]
  (when-some [before-text (not-empty (string/triml (subs line 0 start-col)))]
    (doto (Label. before-text)
      (HBox/setHgrow Priority/ALWAYS)
      (.setPrefWidth Label/USE_COMPUTED_SIZE))))

(defn- make-matched-item-match-text
  ^Label [{:keys [^String line ^long start-col ^long end-col] :as _item}]
  (let [match-text (subs line start-col end-col)]
    (doto (Label. match-text)
      (ui/add-style! "matched")
      (.setMinWidth Label/USE_PREF_SIZE))))

(defn- make-matched-item-after-text
  ^Label [{:keys [^String line ^long end-col] :as _item}]
  (when-some [after-text (not-empty (string/trimr (subs line end-col)))]
    (doto (Label. after-text)
      (HBox/setHgrow Priority/ALWAYS))))

(defn- make-matched-item-graphic [item]
  (let [children (ui/node-array
                   (keep #(% item)
                         [make-matched-item-icon
                          make-matched-item-row-indicator
                          make-matched-item-before-text
                          make-matched-item-match-text
                          make-matched-item-after-text]))]
    (doto (HBox. children)
      (.setPrefWidth 0.0)
      (.setAlignment Pos/CENTER_LEFT))))

(defn- init-search-in-files-tree-view! [^TreeView tree-view]
  (ui/customize-tree-view! tree-view {:double-click-expand? false})
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (.setShowRoot tree-view false)
  (.setRoot tree-view (doto (TreeItem.)
                        (.setExpanded true)))
  (ui/cell-factory! tree-view
                    (fn [item]
                      (if (satisfies? resource/Resource item)
                        {:text (resource/proj-path item)
                         :icon (workspace/resource-icon item)
                         :style (resource/style-classes item)}
                        {:graphic (make-matched-item-graphic item)
                         :style (resource/style-classes (:resource item))}))))

(defn- make-search-in-files-tree-view
  ^TreeView []
  (doto (TreeView.) init-search-in-files-tree-view!))

(defn- make-search-in-files-progress-indicator []
  (doto (ProgressIndicator.)
    (.setMouseTransparent true)
    (AnchorPane/setRightAnchor 10.0)
    (AnchorPane/setBottomAnchor 10.0)
    (.setPrefWidth 28.0)
    (.setPrefHeight 28.0)))

(defn- resolve-search-in-files-tree-view-selection [selection]
  (into []
        (keep (fn [item]
                (if (satisfies? resource/Resource item)
                  (when (resource/exists? item)
                    [item {}])
                  (let [resource (:resource item)]
                    (when (resource/exists? resource)
                      (let [row (:row item)
                            cursor-range (data/->CursorRange (data/->Cursor row (:start-col item))
                                                             (data/->Cursor row (:end-col item)))
                            ;; NOTE:
                            ;; :caret-position is here to support Open as Text.
                            ;; We might want to remove it now that all text
                            ;; files are opened using the code editor.
                            opts {:caret-position (:caret-position item)
                                  :cursor-range cursor-range}]
                        [resource opts]))))))
        selection))

(def ^:private search-in-files-term-prefs-key "search-in-files-term")
(def ^:private search-in-files-exts-prefs-key "search-in-files-exts")
(def ^:private search-in-files-include-libraries-prefs-key "search-in-files-include-libraries")

(defn set-search-term! [prefs term]
  (assert (string? term))
  (prefs/set-prefs prefs search-in-files-term-prefs-key term))

(defn- start-search-in-files! [project prefs results-tab-tree-view results-tab-progress-indicator open-fn show-find-results-fn]
  (let [root      ^Parent (ui/load-fxml "search-in-files-dialog.fxml")
        scene     (Scene. root)
        stage     (doto (ui/make-stage)
                    (.initStyle StageStyle/DECORATED)
                    (.initOwner (ui/main-stage))
                    (.setResizable false))]
    (ui/with-controls root [^TextField search ^TextField types ^CheckBox include-libraries-check-box ^TreeView resources-tree ok search-in-progress]
      (ui/visible! search-in-progress false)
      (let [start-consumer! #(start-tree-update-timer!
                               [resources-tree results-tab-tree-view]
                               [results-tab-progress-indicator search-in-progress]
                               %)
            stop-consumer! ui/timer-stop!
            report-error! (fn [error] (ui/run-later (throw error)))
            file-resource-save-data-future (project-search/make-file-resource-save-data-future report-error! project)
            {:keys [abort-search! start-search!]} (project-search/make-file-searcher file-resource-save-data-future start-consumer! stop-consumer! report-error!)
            on-input-changed! (fn [_ _ _]
                                (let [term (.getText search)
                                      exts (.getText types)
                                      include-libraries? (.isSelected include-libraries-check-box)]
                                  (prefs/set-prefs prefs search-in-files-term-prefs-key term)
                                  (prefs/set-prefs prefs search-in-files-exts-prefs-key exts)
                                  (prefs/set-prefs prefs search-in-files-include-libraries-prefs-key include-libraries?)
                                  (start-search! term exts include-libraries?)))
            dismiss-and-abort-search! (fn []
                                        (abort-search!)
                                        (ui/close! stage))
            dismiss-and-show-find-results! (fn []
                                             (show-find-results-fn)
                                             (ui/close! stage))
            open-selected! (fn []
                             (doseq [[resource opts] (resolve-search-in-files-tree-view-selection (ui/selection resources-tree))]
                               (open-fn resource opts))
                             (dismiss-and-abort-search!))]
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
                               KeyCode/DOWN   (when (and (= TextField (type (ui/focus-owner scene)))
                                                         (not= 0 (.getExpandedItemCount resources-tree)))
                                                (.consume event)
                                                (ui/request-focus! resources-tree))
                               KeyCode/ESCAPE (do (.consume event)
                                                  (dismiss-and-abort-search!))
                               KeyCode/ENTER  (do (.consume event)
                                                  (if (= TextField (type (ui/focus-owner scene)))
                                                    (dismiss-and-show-find-results!)
                                                    (open-selected!)))
                               KeyCode/TAB    (when (and (= types (ui/focus-owner scene))
                                                         (= 0 (.getExpandedItemCount resources-tree)))
                                                (.consume event)
                                                (ui/request-focus! search))
                               nil))))

        (let [term (prefs/get-prefs prefs search-in-files-term-prefs-key "")
              exts (prefs/get-prefs prefs search-in-files-exts-prefs-key "")
              include-libraries? (prefs/get-prefs prefs search-in-files-include-libraries-prefs-key true)]
          (ui/text! search term)
          (ui/text! types exts)
          (ui/value! include-libraries-check-box include-libraries?)
          (start-search! term exts include-libraries?))

        (ui/observe (.textProperty search) on-input-changed!)
        (ui/observe (.textProperty types) on-input-changed!)
        (ui/observe (.selectedProperty include-libraries-check-box) on-input-changed!)
        (.setScene stage scene)
        (ui/show! stage)))))

(g/defnode SearchResultsView
  (inherits core/Scope)
  (property open-resource-fn g/Any)
  (property search-results-container g/Any))

(defn- open-resource! [search-results-view resource opts]
  (let [open-resource-fn (g/node-value search-results-view :open-resource-fn)]
    (open-resource-fn resource opts)))

(defn- update-search-results! [search-results-view ^TreeView results-tree-view ^ProgressIndicator progress-indicator]
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
                               (open-selected!)))))
      (.add progress-indicator))))

(defn make-search-results-view! [view-graph ^AnchorPane search-results-container open-resource-fn]
  (g/make-node! view-graph SearchResultsView
                :open-resource-fn open-resource-fn
                :search-results-container search-results-container))

(defn show-search-in-files-dialog! [search-results-view project prefs show-search-results-tab-fn]
  (let [results-tab-tree-view (make-search-in-files-tree-view)
        progress-indicator (make-search-in-files-progress-indicator)
        open-fn (partial open-resource! search-results-view)
        show-matches-fn (fn []
                          (update-search-results! search-results-view results-tab-tree-view progress-indicator)
                          (show-search-results-tab-fn))]
    (start-search-in-files! project prefs results-tab-tree-view progress-indicator open-fn show-matches-fn)))

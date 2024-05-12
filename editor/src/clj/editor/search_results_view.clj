;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.search-results-view
  (:require [cljfx.api :as fx]
            [cljfx.fx.anchor-pane :as fx.anchor-pane]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.grid-pane :as fx.grid-pane]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.hyperlink :as fx.hyperlink]
            [cljfx.fx.progress-indicator :as fx.progress-indicator]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.tree-item :as fx.tree-item]
            [cljfx.fx.tree-table-cell :as fx.tree-table-cell]
            [cljfx.fx.tree-table-column :as fx.tree-table-column]
            [cljfx.fx.tree-table-view :as fx.tree-table-view]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.core :as core]
            [editor.defold-project-search :as project-search]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.fxui :as fxui]
            [editor.icons :as icons]
            [editor.outline :as outline]
            [editor.prefs :as prefs]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.coll :refer [flipped-pair]]
            [util.fn :as fn])
  (:import [java.util Collection]
           [javafx.animation AnimationTimer]
           [javafx.event Event]
           [javafx.geometry Pos]
           [javafx.scene Parent Scene]
           [javafx.scene.control CheckBox Label ProgressIndicator SelectionMode TextField TreeItem TreeTableView TreeView]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.scene.paint Color]
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
                          (fn [^AnimationTimer timer elapsed-time _]
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
      (icons/get-image-view 16)))

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

(defn- resource-cell [{:keys [resource qualifier]}]
  {:graphic {:fx/type fx.h-box/lifecycle
             :spacing 6
             :children (cond-> [{:fx/type fxui/label
                                 :text (resource/resource->proj-path resource)}]
                               qualifier
                               (conj {:fx/type fxui/label
                                      :style {:-fx-text-fill :-df-text-darker}
                                      :text qualifier}))}})

(defn- property->edit-type-dispatch-value [property]
  (let [edit-type (-> property :edit-type :type (or (:type property)))]
    (or (g/value-type-dispatch-value edit-type)
        edit-type)))

(defmulti property-column-suffix property->edit-type-dispatch-value)
(defmethod property-column-suffix :default [_] "")
(defmethod property-column-suffix types/Vec2 [property]
  (str ": " (string/join " " (:labels property ["X" "Y"]))))
(defmethod property-column-suffix types/Vec3 [property]
  (str ": " (string/join " " (:labels property ["X" "Y" "Z"]))))
(defmethod property-column-suffix types/Vec4 [property]
  (str ": " (string/join " " (:labels property ["X" "Y" "Z" "W"]))))

(defmulti override-value-cell-view property->edit-type-dispatch-value)

(defn- overridden-style-classes [property]
  (if (contains? property :original-value)
    ["overridden"]
    []))

(defmethod override-value-cell-view :default [{:keys [value] :as property}]
  {:fx/type fx.h-box/lifecycle
   :alignment (if (number? value) :top-right :top-left)
   :children [{:fx/type fxui/label
               :h-box/hgrow :always
               :style-class (overridden-style-classes property)
               :text (string/replace (str value) \newline \space)}]})

(defmethod override-value-cell-view g/Bool [{:keys [value] :as property}]
  {:fx/type fx.check-box/lifecycle
   :disable true
   :style-class (into ["check-box" "override-inspector-check-box"] (overridden-style-classes property))
   :selected (boolean value)})

(defn- multi-text-field [{:keys [value] :as property}]
  {:fx/type fx.grid-pane/lifecycle
   :hgap 5
   :column-constraints (repeat (count value)
                               {:fx/type fx.column-constraints/lifecycle
                                :percent-width (/ 100.0 (max 1 (count value)))})
   :children (into []
                   (map-indexed
                     (fn [i v]
                       {:fx/type fxui/label
                        :grid-pane/column i
                        :grid-pane/halignment :right
                        :style-class (overridden-style-classes property)
                        :text (field-expression/format-number v)}))
                   value)})

(defmethod override-value-cell-view types/Vec2 [property]
  (multi-text-field property))

(defmethod override-value-cell-view types/Vec3 [property]
  (multi-text-field property))

(defmethod override-value-cell-view types/Vec4 [property]
  (multi-text-field property))

(defmethod override-value-cell-view types/Color [{:keys [value] :as property}]
  (if value
    (let [[r g b a] value
          color (Color. r g b a)
          nr (Math/round (double (* 255 r)))
          ng (Math/round (double (* 255 g)))
          nb (Math/round (double (* 255 b)))
          na (Math/round (double (* 255 a)))]
      {:fx/type fxui/label
       :style-class (overridden-style-classes property)
       :graphic {:fx/type fx.region/lifecycle
                 :min-width 10
                 :min-height 10
                 :background {:fills [{:fill color}]}}
       :text (if (= 255 na)
               (format "#%02x%02x%02x" nr ng nb)
               (format "#%02x%02x%02x%02x" nr ng nb na))})
    {:fx/type fxui/label}))

(defmethod override-value-cell-view :choicebox [{:keys [edit-type value] :as property}]
  (let [labels (into {} (:options edit-type))]
    {:fx/type fxui/label
     :style-class (overridden-style-classes property)
     :text (get labels value)}))

(defn open-hyperlink-resource! [open-resource-fn resource _]
  (open-resource-fn resource {}))

(defmethod override-value-cell-view resource/Resource [{:keys [value open-resource-fn] :as property}]
  {:fx/type fx.hyperlink/lifecycle
   :style-class (into ["override-inspector-hyperlink"] (overridden-style-classes property))
   :on-action (fn/partial #'open-hyperlink-resource! open-resource-fn value)
   :text (resource/resource->proj-path value)})

(defn- value-cell [open-resource-fn property]
  {:graphic (override-value-cell-view (assoc property :value (properties/value property)
                                                      :open-resource-fn open-resource-fn))})

(defn- property-value [property-keyword tree]
  (-> tree :properties property-keyword))

(defn- tree-table-view-event-filter [open-resource-fn ^Event e]
  (when (and (instance? MouseEvent e)
             (= MouseEvent/MOUSE_PRESSED (.getEventType e)))
    (let [^MouseEvent e e]
      (when (even? (.getClickCount e))
        (.consume e)
        (let [^TreeTableView tree-view (.getSource e)]
          (when-let [^TreeItem tree-item (.getSelectedItem (.getSelectionModel tree-view))]
            (when-let [tree (.getValue tree-item)]
              (open-resource-fn (:resource tree) {:select-node (:node-id tree)}))))))))

(defn- override-inspector-view [state parent open-resource-fn]
  (letfn [(->tree-item [tree]
            {:fx/type fx.tree-item/lifecycle
             :expanded true
             :value tree
             :children (mapv ->tree-item (:children tree))})]
    {:fx/type fxui/ext-with-anchor-pane-props
     :desc {:fx/type fxui/ext-value
            :value parent}
     :props
     {:children
      [{:fx/type fx.anchor-pane/lifecycle
        :anchor-pane/left 0
        :anchor-pane/right 0
        :anchor-pane/top 0
        :anchor-pane/bottom 0
        :children
        [(if (= :search state)
           {:fx/type fx.progress-indicator/lifecycle
            :anchor-pane/right 10
            :anchor-pane/bottom 10
            :pref-width 28
            :pref-height 28
            :mouse-transparent true}
           {:fx/type fx.tree-table-view/lifecycle
            :anchor-pane/bottom 0
            :anchor-pane/top 0
            :anchor-pane/left 0
            :anchor-pane/right 0
            :fixed-cell-size 24
            :event-filter (fn/partial #'tree-table-view-event-filter open-resource-fn)
            :columns (into [{:fx/type fx.tree-table-column/lifecycle
                             :text "Resource"
                             :cell-value-factory identity
                             :cell-factory {:fx/cell-type fx.tree-table-cell/lifecycle
                                            :describe #'resource-cell}}]
                           (map (fn [property-keyword]
                                  {:fx/type fx.tree-table-column/lifecycle
                                   :text (str (properties/keyword->name property-keyword)
                                              (property-column-suffix (property-value property-keyword state)))
                                   :cell-value-factory (fn/partial #'property-value property-keyword)
                                   :cell-factory {:fx/cell-type fx.tree-table-cell/lifecycle
                                                  :describe (fn/partial #'value-cell open-resource-fn)}}))
                           (:display-order state))
            :root (->tree-item state)})]}]}}))

(defn- make-override-tree [node-id properties {:keys [basis] :as evaluation-context}]
  (let [property-pred (if (= :all properties)
                        any?
                        (set properties))]
    (letfn [(make-tree
              ([node-id]
               (make-tree node-id false))
              ([node-id root]
               (let [property-map (:properties (g/node-value node-id :_properties evaluation-context))
                     overridden-properties (into #{}
                                                 (keep (fn [[k property]]
                                                         (when (and (property-pred k)
                                                                    (properties/visible? property)
                                                                    (contains? property :original-value))
                                                           k)))
                                                 property-map)
                     children (into []
                                    (keep make-tree)
                                    (g/overrides basis node-id))]
                 (when (or root
                           (pos? (count overridden-properties))
                           (pos? (count children)))
                   (let [owner-node-id (or (resource-node/owner-resource-node-id basis node-id)
                                           (throw (ex-info "Can't find the owner resource node for an override node"
                                                           {:node-id node-id})))
                         resource (resource-node/resource basis owner-node-id)
                         outline-ids (when (g/node-instance? basis outline/OutlineNode owner-node-id)
                                       (->> (g/node-value owner-node-id :node-outline evaluation-context)
                                            (tree-seq :children :children)
                                            (into #{} (map :node-id))))
                         select-node-id (or (when outline-ids
                                              (loop [node-id node-id]
                                                (cond
                                                  (nil? node-id) nil
                                                  (outline-ids node-id) node-id
                                                  :else (recur (core/owner-node-id basis node-id)))))
                                            node-id)
                         select-node-type (g/node-type* basis select-node-id)
                         qualifier (cond
                                     (g/has-output? select-node-type :url)
                                     (str (g/node-value select-node-id :url evaluation-context))

                                     (g/has-output? select-node-type :id)
                                     (str (g/node-value select-node-id :id evaluation-context))

                                     :else
                                     nil)]
                     {:node-id select-node-id
                      :qualifier qualifier
                      :resource resource
                      :properties property-map
                      :overridden-properties overridden-properties
                      :children children})))))]
      (make-tree node-id true))))

(defn show-override-inspector!
  "Show override inspector tree table in a Search Results view

  Args:
    search-results-view    node id of a search result view
    node-id                root node id whose overrides are inspected
    properties             either :all or a coll of property keywords to include
                           in the override inspector output"
  [search-results-view node-id properties]
  (let [evaluation-context (g/make-evaluation-context)
        parent (g/node-value search-results-view :search-results-container evaluation-context)
        display-order (into {}
                            (map-indexed flipped-pair)
                            (:display-order (g/node-value node-id :_properties evaluation-context)))
        open-resource-fn (partial open-resource! search-results-view)
        renderer (fx/create-renderer
                   :error-handler error-reporting/report-exception!
                   :middleware
                   (comp
                     fxui/wrap-dedupe-desc
                     (fx/wrap-map-desc #'override-inspector-view parent open-resource-fn)))]
    (renderer :search)
    (future
      (try
        (let [tree (make-override-tree node-id properties evaluation-context)]
          (renderer
            (assoc tree
              :display-order (->> (tree-seq :children :children tree)
                                  (into #{}
                                        (comp
                                          (drop 1)
                                          (mapcat :overridden-properties)))
                                  (sort-by display-order)
                                  vec)))
          (g/update-cache-from-evaluation-context! evaluation-context))
        (catch Throwable ex
          (error-reporting/report-exception! ex))))))
;; Copyright 2020-2026 The Defold Foundation
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
            [cljfx.ext.tree-table-view :as fx.ext.tree-table-view]
            [cljfx.fx.anchor-pane :as fx.anchor-pane]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.grid-pane :as fx.grid-pane]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.hyperlink :as fx.hyperlink]
            [cljfx.fx.menu :as fx.menu]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.progress-indicator :as fx.progress-indicator]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.separator :as fx.separator]
            [cljfx.fx.tree-item :as fx.tree-item]
            [cljfx.fx.tree-table-cell :as fx.tree-table-cell]
            [cljfx.fx.tree-table-column :as fx.tree-table-column]
            [cljfx.fx.tree-table-view :as fx.tree-table-view]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.defold-project-search :as project-search]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.fxui :as fxui]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.menu-items :as menu-items]
            [editor.outline :as outline]
            [editor.prefs :as prefs]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.util :as util]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [flipped-pair pair]]
            [util.fn :as fn]
            [util.thread-util :as thread-util])
  (:import [java.util Collection]
           [javafx.animation AnimationTimer]
           [javafx.event Event]
           [javafx.geometry Pos]
           [javafx.scene Parent Scene]
           [javafx.scene.control CheckBox Label ProgressIndicator SelectionMode TextField TreeItem TreeTableView TreeView]
           [javafx.scene.input KeyCode KeyEvent MouseButton MouseEvent]
           [javafx.scene.layout AnchorPane HBox Priority]
           [javafx.scene.paint Color]
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

(defn- make-matched-text-item-row-indicator
  ^Label [{:keys [^long row] :as _item}]
  (doto (Label. (str (inc row) ": "))
    (ui/add-style! "line-number")
    (.setMinWidth Label/USE_PREF_SIZE)))

(defn- make-matched-text-item-before-text
  ^Label [{:keys [^String text ^long start-col] :as _item}]
  (when-some [before-text (not-empty (string/triml (subs text 0 start-col)))]
    (doto (Label. before-text)
      (HBox/setHgrow Priority/ALWAYS)
      (.setPrefWidth Label/USE_COMPUTED_SIZE))))

(defn- make-matched-text-item-match-text
  ^Label [{:keys [^String text ^long start-col ^long end-col] :as _item}]
  (let [match-text (subs text start-col end-col)]
    (doto (Label. match-text)
      (ui/add-style! "matched")
      (.setMinWidth Label/USE_PREF_SIZE))))

(defn- make-matched-text-item-after-text
  ^Label [{:keys [^String text ^long end-col] :as _item}]
  (when-some [after-text (not-empty (string/trimr (subs text end-col)))]
    (doto (Label. after-text)
      (HBox/setHgrow Priority/ALWAYS))))

(defn- make-matched-item-value-path
  ^Label [path-tokens]
  (doto (Label. (str (string/join " \u2192 " path-tokens) ": ")) ; "->" (RIGHTWARDS ARROW)
    (ui/add-style! "value-path")
    (.setMinWidth Label/USE_PREF_SIZE)))

(defn- make-matched-protobuf-item-value-path
  ^Label [{:keys [path] :as _item}]
  (make-matched-item-value-path
    (map (fn [token]
           (cond-> token
                   (keyword? token)
                   (protobuf/keyword->field-name)))
         path)))

(defn- make-matched-setting-item-value-path
  ^Label [{:keys [path] :as _item}]
  (make-matched-item-value-path path))

(defn- make-matched-item-graphic-impl
  ^HBox [item child-node-fns]
  (let [children (ui/node-array
                   (keep #(% item)
                         child-node-fns))]
    (doto (HBox. children)
      (.setPrefWidth 0.0)
      (.setAlignment Pos/CENTER_LEFT))))

(defmulti ^:private make-matched-item-graphic :match-type)

(defmethod make-matched-item-graphic :match-type-text [item]
  (make-matched-item-graphic-impl
    item
    [make-matched-text-item-row-indicator
     make-matched-text-item-before-text
     make-matched-text-item-match-text
     make-matched-text-item-after-text]))

(defmethod make-matched-item-graphic :match-type-protobuf [item]
  (make-matched-item-graphic-impl
    item
    [make-matched-protobuf-item-value-path
     make-matched-text-item-before-text
     make-matched-text-item-match-text
     make-matched-text-item-after-text]))

(defmethod make-matched-item-graphic :match-type-setting [item]
  (make-matched-item-graphic-impl
    item
    [make-matched-setting-item-value-path
     make-matched-text-item-before-text
     make-matched-text-item-match-text
     make-matched-text-item-after-text]))

(defmulti ^:private make-matched-item-open-opts :match-type)

(defmethod make-matched-item-open-opts :match-type-text [item]
  (let [row (:row item)
        cursor-range (data/->CursorRange (data/->Cursor row (:start-col item))
                                         (data/->Cursor row (:end-col item)))]
    ;; NOTE:
    ;; :caret-position is here to support Open as Text.
    ;; We might want to remove it now that all text
    ;; files are opened using the code editor.
    {:caret-position (:caret-position item)
     :cursor-range cursor-range}))

(defmethod make-matched-item-open-opts :match-type-protobuf [item]
  ;; TODO: Make the view select and focus on the matching property.
  {:value-path (:path item)})

(defmethod make-matched-item-open-opts :match-type-setting [item]
  ;; TODO: Make the view select and focus on the matching property.
  {:value-path (:path item)})

(defn- init-search-in-files-tree-view! [^TreeView tree-view]
  (ui/customize-tree-view! tree-view {:double-click-expand? false})
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (.setShowRoot tree-view false)
  (.setRoot tree-view (doto (TreeItem.)
                        (.setExpanded true)))
  (ui/cell-factory! tree-view
                    (fn [item]
                      (if (resource/resource? item)
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
                (if (resource/resource? item)
                  (when (resource/exists? item)
                    [item {}])
                  (let [resource (:resource item)]
                    (when (resource/exists? resource)
                      (let [opts (make-matched-item-open-opts item)]
                        [resource opts]))))))
        selection))

(def ^:private search-in-files-term-prefs-key [:search-in-files :term])
(def ^:private search-in-files-exts-prefs-key [:search-in-files :exts])
(def ^:private search-in-files-include-libraries-prefs-key [:search-in-files :include-libraries])

(defn set-search-term! [prefs term]
  (assert (string? term))
  (prefs/set! prefs search-in-files-term-prefs-key term))

(defn- start-search-in-files! [project prefs localization results-tab-tree-view results-tab-progress-indicator open-fn show-find-results-fn]
  (let [root      ^Parent (ui/load-fxml "search-in-files-dialog.fxml")
        scene     (Scene. root)
        stage     (doto (ui/make-stage)
                    (.initStyle StageStyle/DECORATED)
                    (.initOwner (ui/main-stage))
                    (.setResizable false))]
    (ui/with-controls root [search-label types-label ^TextField search ^TextField types ^CheckBox include-libraries-check-box ^TreeView resources-tree ok search-in-progress]
      (ui/visible! search-in-progress false)
      (let [start-consumer! #(start-tree-update-timer!
                               [resources-tree results-tab-tree-view]
                               [results-tab-progress-indicator search-in-progress]
                               %)
            stop-consumer! ui/timer-stop!
            report-error! (fn [error] (ui/run-later (throw error)))
            workspace (project/workspace project)
            search-data-future (project-search/make-search-data-future report-error! project)
            {:keys [abort-search! start-search!]} (project-search/make-file-searcher workspace search-data-future start-consumer! stop-consumer! report-error!)
            on-input-changed! (fn [_ _ _]
                                (let [term (.getText search)
                                      exts (.getText types)
                                      include-libraries? (.isSelected include-libraries-check-box)]
                                  (prefs/set! prefs search-in-files-term-prefs-key term)
                                  (prefs/set! prefs search-in-files-exts-prefs-key exts)
                                  (prefs/set! prefs search-in-files-include-libraries-prefs-key include-libraries?)
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
        (localization/localize! (.titleProperty stage) localization (localization/message "dialog.search-in-files.title"))
        (localization/localize! search-label localization (localization/message "dialog.search-in-files.label.search"))
        (localization/localize! types-label localization (localization/message "dialog.search-in-files.label.types"))
        (localization/localize! include-libraries-check-box localization (localization/message "dialog.search-in-files.label.include-libraries"))
        (localization/localize! ok localization (localization/message "dialog.search-in-files.button.keep-results"))
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

        (let [term (prefs/get prefs search-in-files-term-prefs-key)
              exts (prefs/get prefs search-in-files-exts-prefs-key)
              include-libraries? (prefs/get prefs search-in-files-include-libraries-prefs-key)]
          (ui/text! search term)
          (ui/text! types exts)
          (ui/value! include-libraries-check-box include-libraries?)
          (start-search! term exts include-libraries?))

        (ui/observe (.textProperty search) on-input-changed!)
        (ui/observe (.textProperty types) on-input-changed!)
        (ui/observe (.selectedProperty include-libraries-check-box) on-input-changed!)
        (.setScene stage scene)
        (ui/show! stage localization)))))

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

(defn show-search-in-files-dialog! [search-results-view project prefs localization show-search-results-tab-fn]
  (let [results-tab-tree-view (make-search-in-files-tree-view)
        progress-indicator (make-search-in-files-progress-indicator)
        open-fn (partial open-resource! search-results-view)
        show-matches-fn (fn []
                          (update-search-results! search-results-view results-tab-tree-view progress-indicator)
                          (show-search-results-tab-fn))]
    (start-search-in-files! project prefs localization results-tab-tree-view progress-indicator open-fn show-matches-fn)))

(defn- resource-cell-view [proj-path qualifier]
  {:fx/type fx.h-box/lifecycle
   :spacing 6
   :children (cond-> [{:fx/type fxui/legacy-label
                       :wrap-text false
                       :text proj-path}]
                     qualifier
                     (conj {:fx/type fxui/legacy-label
                            :style {:-fx-text-fill :-df-text-dark}
                            :wrap-text false
                            :text qualifier}))})

(defn- resource-cell [{:keys [resource qualifier]}]
  (let [proj-path (resource/resource->proj-path resource)]
    {:graphic (resource-cell-view proj-path qualifier)}))

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
   :children [{:fx/type fxui/legacy-label
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
                       {:fx/type fxui/legacy-label
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
      {:fx/type fxui/legacy-label
       :style-class (overridden-style-classes property)
       :graphic {:fx/type fx.region/lifecycle
                 :min-width 10
                 :min-height 10
                 :background {:fills [{:fill color}]}}
       :text (if (= 255 na)
               (format "#%02x%02x%02x" nr ng nb)
               (format "#%02x%02x%02x%02x" nr ng nb na))})
    {:fx/type fxui/legacy-label}))

(defmethod override-value-cell-view :choicebox [{:keys [edit-type value] :as property}]
  (let [labels (into {} (:options edit-type))]
    {:fx/type fxui/legacy-label
     :style-class (overridden-style-classes property)
     :text (get labels value)}))

(defmethod override-value-cell-view resource/Resource [{:keys [value] :as property}]
  {:fx/type fx.hyperlink/lifecycle
   :style-class (into ["override-inspector-hyperlink"] (overridden-style-classes property))
   :on-action {:event-type :on-click-resource
               :resource value}
   :text (resource/resource->proj-path value)})

(defn- value-cell [property]
  {:graphic (override-value-cell-view (assoc property :value (properties/value property)))})

(defn- property-value [property-keyword state]
  (-> state :properties property-keyword))

(defmulti handle-override-inspector-event :event-type)

(defmethod handle-override-inspector-event :on-refresh-view [{:keys [^Event fx/event]}]
  (.consume event)
  [[:refresh-view nil]])

(defmethod handle-override-inspector-event :on-click-resource [{:keys [^Event fx/event resource]}]
  (.consume event)
  [[:open-resource {:resource resource}]])

(defmethod handle-override-inspector-event :on-click-table [{:keys [^Event fx/event]}]
  (when (and (= MouseEvent/MOUSE_PRESSED (.getEventType event))
             (= MouseButton/PRIMARY (.getButton ^MouseEvent event))
             (even? (.getClickCount ^MouseEvent event)))
    (.consume event)
    (let [^TreeTableView tree-table-view (.getSource event)]
      (when-let [^TreeItem tree-item (.getSelectedItem (.getSelectionModel tree-table-view))]
        (when-let [{:keys [resource node-id]} (.getValue tree-item)]
          [[:open-resource (cond-> {:resource resource}
                                   node-id (assoc :opts {:select-node node-id}))]])))))

(defmethod handle-override-inspector-event :on-transfer-overrides [{:keys [transfer-overrides-plan]}]
  [[:transfer-overrides transfer-overrides-plan]])

(defmethod handle-override-inspector-event :on-select-item [{:keys [^TreeItem fx/event]}]
  [[:select-item (some-> event .getValue)]])

(defn- ->tree-item [tree]
  {:fx/type fx.tree-item/lifecycle
   :expanded true
   :value tree
   :children (mapv ->tree-item (:children tree))})

(defn- override-inspector-tool-bar
  [{:keys [pull-up-overrides-menu-items
           push-down-overrides-menu-items]}]
  {:fx/type fx.v-box/lifecycle
   :style-class "override-inspector-tool-bar"
   :children
   [{:fx/type fxui/legacy-button
     :variant :icon
     :on-action {:event-type :on-refresh-view}
     :graphic {:fx/type fxui/icon-graphic
               :type :icon/refresh
               :size 20.0}}
    {:fx/type fx.separator/lifecycle}
    {:fx/type fxui/legacy-menu-button
     :variant :icon
     :graphic {:fx/type fxui/icon-graphic
               :type :icon/pull-up-override
               :size 20.0}
     :popup-side :right
     :disable (coll/empty? pull-up-overrides-menu-items)
     :items pull-up-overrides-menu-items}
    {:fx/type fxui/legacy-menu-button
     :variant :icon
     :graphic {:fx/type fxui/icon-graphic
               :type :icon/push-down-override
               :size 20.0}
     :popup-side :right
     :disable (coll/empty? push-down-overrides-menu-items)
     :items push-down-overrides-menu-items}]})

(defn- override-inspector-query-label [state]
  (let [{:keys [proj-path qualifier prop-kws localization]} state
        qualifier (or qualifier "none")
        proj-path (or proj-path "none")]
    {:fx/type fxui/ext-localize
     :localization localization
     :message (case (count prop-kws)
                0 (localization/message "search.overrides.title.all" {"qualifier" qualifier "resource" proj-path})
                (1 2 3 4) (localization/message "search.overrides.title.list"
                                                {"count" (count prop-kws)
                                                 "list" (localization/and-list (mapv properties/keyword->name prop-kws))
                                                 "qualifier" qualifier
                                                 "resource" proj-path})
                (localization/message "search.overrides.title.specific" {"qualifier" qualifier "resource" proj-path}))
     :desc {:fx/type fxui/legacy-label
            :wrap-text false
            :style {:-fx-min-height 26.0}}}))

(defn- override-inspector-tree-table-view
  [{:keys [display-order
           pull-up-overrides-menu-items
           push-down-overrides-menu-items
           tree
           localization]}]
  (let [tree-table-columns
        (into [{:fx/type fxui/ext-localize
                :localization localization
                :message (localization/message "search.column.resource")
                :desc {:fx/type fx.tree-table-column/lifecycle
                       :reorderable false
                       :cell-value-factory identity
                       :cell-factory {:fx/cell-type fx.tree-table-cell/lifecycle
                                      :describe #'resource-cell}}}]
              (map (fn [property-keyword]
                     {:fx/type fx.tree-table-column/lifecycle
                      :text (str (properties/keyword->name property-keyword)
                                 (property-column-suffix (property-value property-keyword tree)))
                      :reorderable false
                      :cell-value-factory (fn/partial #'property-value property-keyword)
                      :cell-factory {:fx/cell-type fx.tree-table-cell/lifecycle
                                     :describe #'value-cell}}))
              display-order)

        transfer-overrides-context-menu-items
        (cond-> []

                (coll/not-empty pull-up-overrides-menu-items)
                (conj {:fx/type fxui/ext-localize
                       :localization localization
                       :message menu-items/pull-up-overrides-message
                       :desc {:fx/type fx.menu/lifecycle
                              :items pull-up-overrides-menu-items}})

                (coll/not-empty push-down-overrides-menu-items)
                (conj {:fx/type fxui/ext-localize
                       :localization localization
                       :message menu-items/push-down-overrides-message
                       :desc {:fx/type fx.menu/lifecycle
                              :items push-down-overrides-menu-items}}))

        context-menu
        (when (coll/not-empty transfer-overrides-context-menu-items)
          {:fx/type fx.context-menu/lifecycle
           :items transfer-overrides-context-menu-items})]

    {:fx/type fx.ext.tree-table-view/with-selection-props
     :props {:selection-mode :single
             :on-selected-item-changed {:event-type :on-select-item}}
     :desc
     (cond-> {:fx/type fx.tree-table-view/lifecycle
              :fixed-cell-size 24
              :event-filter {:event-type :on-click-table}
              :columns tree-table-columns
              :root (->tree-item tree)}

             context-menu
             (assoc :context-menu context-menu))}))

(defn- transfer-overrides-plan-menu-item [transfer-overrides-plan localization evaluation-context]
  {:fx/type fxui/ext-localize
   :localization localization
   :message (properties/transfer-overrides-description transfer-overrides-plan evaluation-context)
   :desc {:fx/type fx.menu-item/lifecycle
          :disable (not (properties/can-transfer-overrides? transfer-overrides-plan))
          :on-action {:event-type :on-transfer-overrides
                      :transfer-overrides-plan transfer-overrides-plan}}})

(defn- override-inspector-view [state parent localization]
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
      [(if (= :searching (:progress state))
         {:fx/type fx.progress-indicator/lifecycle
          :anchor-pane/right 10
          :anchor-pane/bottom 10
          :pref-width 28
          :pref-height 28
          :mouse-transparent true}
         (let [{:keys [display-order queried-properties selected-item tree]} state
               queried-proj-path (some-> tree :resource resource/proj-path)
               queried-qualifier (:qualifier tree)
               source-node-id (:node-id selected-item)
               overridden-prop-kws (:overridden-properties selected-item)

               [pull-up-overrides-menu-items push-down-overrides-menu-items]
               (when (and source-node-id (coll/not-empty overridden-prop-kws))
                 (g/with-auto-evaluation-context evaluation-context
                   (when-let [source-prop-infos-by-prop-kw (properties/transferred-properties source-node-id overridden-prop-kws evaluation-context)]
                     (pair (mapv #(transfer-overrides-plan-menu-item % localization evaluation-context)
                                 (properties/pull-up-overrides-plan-alternatives source-node-id source-prop-infos-by-prop-kw evaluation-context))
                           (mapv #(transfer-overrides-plan-menu-item % localization evaluation-context)
                                 (properties/push-down-overrides-plan-alternatives source-node-id source-prop-infos-by-prop-kw evaluation-context))))))]

           {:fx/type fx.h-box/lifecycle
            :anchor-pane/bottom 0
            :anchor-pane/top 0
            :anchor-pane/left 0
            :anchor-pane/right 0
            :children
            [{:fx/type override-inspector-tool-bar
              :h-box/hgrow :never
              :pull-up-overrides-menu-items pull-up-overrides-menu-items
              :push-down-overrides-menu-items push-down-overrides-menu-items}
             {:fx/type fx.v-box/lifecycle
              :h-box/hgrow :always
              :alignment :top-center
              :children
              [{:fx/type override-inspector-query-label
                :v-box/vgrow :never
                :proj-path queried-proj-path
                :qualifier queried-qualifier
                :prop-kws queried-properties
                :localization localization}
               {:fx/type override-inspector-tree-table-view
                :v-box/vgrow :always
                :display-order display-order
                :pull-up-overrides-menu-items pull-up-overrides-menu-items
                :push-down-overrides-menu-items push-down-overrides-menu-items
                :tree tree
                :localization localization}]}]}))]}]}})

(defn- make-override-tree [node-id property-pred {:keys [basis] :as evaluation-context}]
  (letfn [(make-tree
            ([node-id]
             (make-tree node-id false))
            ([node-id root]
             (thread-util/throw-if-interrupted!)
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
                       qualifier (gu/node-qualifier-label select-node-id evaluation-context)]
                   {:node-id select-node-id
                    :qualifier qualifier
                    :resource resource
                    :properties property-map
                    :overridden-properties overridden-properties
                    :children children})))))]
    (make-tree node-id true)))

(defn show-override-inspector!
  "Show override inspector tree table in a Search Results view

  Args:
    search-results-view    node id of a search result view
    node-id                root node id whose overrides are inspected
    properties             either :all or a coll of property keywords to include
                           in the override inspector output
    localization           the Localization instance"
  [search-results-view node-id properties localization]
  (let [[parent property-keyword->display-order]
        (g/with-auto-evaluation-context evaluation-context
          [(g/node-value search-results-view :search-results-container evaluation-context)
           (into {}
                 (map-indexed flipped-pair)
                 (:display-order (g/node-value node-id :_properties evaluation-context)))])

        state-atom (atom {})
        search-future-atom (atom nil)

        refresh-view!
        (fn refresh-view! []
          (swap! state-atom assoc :progress :searching)
          (let [evaluation-context (g/make-evaluation-context)

                search-future
                (future
                  (try
                    (let [has-specific-properties (not= :all properties)
                          property-pred (if has-specific-properties (set properties) any?)
                          tree (make-override-tree node-id property-pred evaluation-context)

                          display-order
                          (->> (if has-specific-properties
                                 properties
                                 (coll/into-> (tree-seq :children :children tree) #{}
                                   (drop 1)
                                   (mapcat :overridden-properties)))
                               (sort-by property-keyword->display-order)
                               (vec))

                          queried-properties
                          (when has-specific-properties
                            display-order)]
                      (ui/run-later
                        (g/update-cache-from-evaluation-context! evaluation-context))
                      (swap! state-atom assoc
                             :queried-properties queried-properties
                             :progress :done
                             :selected-item nil
                             :tree tree
                             :display-order display-order))
                    (catch InterruptedException _
                      nil)
                    (catch Throwable ex
                      (error-reporting/report-exception! ex))
                    (finally
                      (reset! search-future-atom nil))))]
            (when-some [old-search-future (thread-util/preset! search-future-atom search-future)]
              (future-cancel old-search-future))
            nil))

        event-handler
        (fx/wrap-effects
          handle-override-inspector-event
          {:refresh-view (fn [_ _]
                           (refresh-view!))
           :transfer-overrides (fn [transfer-overrides-plan _]
                                 (properties/transfer-overrides! transfer-overrides-plan)
                                 (refresh-view!))
           :open-resource (fn [{:keys [resource opts]} _]
                            (open-resource! search-results-view resource opts))
           :select-item (fn [item _]
                          (swap! state-atom assoc :selected-item item))})

        renderer
        (fx/create-renderer
          :error-handler error-reporting/report-exception!
          :middleware (comp
                        fxui/wrap-dedupe-desc
                        (fx/wrap-map-desc #'override-inspector-view parent localization))
          :opts {:fx.opt/map-event-handler event-handler})]

    (refresh-view!)
    (fx/mount-renderer state-atom renderer)))

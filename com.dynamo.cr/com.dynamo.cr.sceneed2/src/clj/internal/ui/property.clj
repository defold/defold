(ns internal.ui.property
  (:require [clojure.string :as str]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [internal.node :as in]
            [internal.query :as iq]
            [camel-snake-kebab :refer :all])
  (:import [org.eclipse.core.runtime IStatus Status]
           [org.eclipse.swt SWT]
           [org.eclipse.swt.custom StackLayout]
           [org.eclipse.swt.layout FillLayout GridData GridLayout]
           [org.eclipse.swt.widgets Composite]
           [org.eclipse.ui ISelectionListener]
           [org.eclipse.ui.forms.widgets FormToolkit]
           [com.dynamo.cr.properties Messages]))

(set! *warn-on-reflection* true)

;; TODO: This should actually **aggregate** something.
(defnk aggregate-properties
  [properties]
  (apply merge {} properties))

(defn- niceify-label
  [k]
  (-> k
    name
    ->Camel_Snake_Case_String
    (str/replace "_" " ")))

(defn- property-control-strip
  [[prop-name {:keys [value]}]]
  (let [label-text (niceify-label prop-name)
        value-text (pr-str value)]
    [[:label-composite {:type :composite
                        :layout {:type :stack}
                        :children [[:label      {:type :label :text label-text}]
                                   [:label-link {:type :hyperlink :text label-text :underlined true :on-click (fn [_] (prn "RESET " prop-name)) :foreground [0 0 255] :tooltip-text Messages/FormPropertySheetViewer_RESET_VALUE}]]}]
     [:label           {:type :label :text value-text :layout-data {:type :grid :grab-excess-horizontal-space true :horizontal-alignment SWT/FILL :width-hint 50}}]
     [:dummy           {:type :label :layout-data {:type :grid :exclude true}}]
     [:status-label    {:type :status-label :style :border :status Status/OK_STATUS :layout-data {:type :grid :grab-excess-horizontal-space true :horizontal-fill SWT/HORIZONTAL :width-hint 50 :exclude true}}]]))

(defn- make-property-page
  [toolkit widget-tree properties]
  (-> (ui/make-control toolkit (ui/widget widget-tree [:form :composite])
        [:grid-container
         {:type :composite
          :layout {:type :grid :num-columns 2 :margin-width 0}
          :children (mapcat property-control-strip properties)}])
    (ui/widget [:grid-container])))

(defn- make-empty-property-page
  [toolkit widget-tree]
  (-> (ui/make-control toolkit (ui/widget widget-tree [:form :composite])
        [:no-selection
         {:type :composite
          :layout {:type :grid}
          :children [[:no-selection-label {:type :label :text Messages/FormPropertySheetViewer_NO_PROPERTIES}]]}])
    (ui/widget [:no-selection])))

(defn- cache-key
  [properties]
  (map-vals :type properties))

(defn- lookup-or-create [cache key f & args]
  (-> cache
      (swap! (fn [cache]
                (if (contains? cache key)
                  cache
                  (assoc cache key (apply f args)))))
      (get key)))

(defn- refresh-property-page
  [{:keys [sheet-cache toolkit widget-tree] :as node}]
  (let [properties (in/get-node-value node :content)]
    (-> sheet-cache
        (lookup-or-create (cache-key properties) make-property-page toolkit widget-tree properties)
        (ui/bring-to-front)))
  (ui/scroll-to-top (ui/widget widget-tree [:form])))

(defn- refresh-after-a-while
  [graph this transaction]
  (when (ds/is-modified? transaction this :content)
    ;; TODO: coalesce refreshes
    (ui/after 100 (refresh-property-page (ds/refresh this)))))

(def gui
  [:form {:type   :form
          :text   "Properties"
          :layout {:type :grid}
          :children [[:composite
                      {:type :composite
                       :layout-data {:type :grid
                                     :horizontal-alignment SWT/FILL
                                     :vertical-alignment SWT/FILL
                                     :grab-excess-vertical-space true
                                     :grab-excess-horizontal-space true}
                       :layout {:type :stack}}]]}])

(n/defnode PropertyView
  (input properties [t/Properties])

  (property triggers t/Triggers (default [#'refresh-after-a-while]))

  (output content s/Any aggregate-properties)

  (on :create
    (let [toolkit      (FormToolkit. (.getDisplay ^Composite (:parent event)))
          widget-tree  (ui/make-control toolkit (:parent event) gui)
          sheet-cache  (atom {})]
      (lookup-or-create sheet-cache (cache-key {}) make-empty-property-page toolkit widget-tree)
      (ds/set-property self
        :sheet-cache  sheet-cache
        :toolkit      toolkit
        :widget-tree  widget-tree)))

  ISelectionListener
  (selectionChanged [this part selection]
    (ds/transactional
      (doseq [[source-node source-label] (iq/sources-of this :properties)]
        (ds/disconnect source-node source-label this :properties))
      (doseq [n @selection]
        (ds/connect {:_id n} :properties this :properties)))))

(defn implementation-for
  [scope]
  (ds/transactional
    (ds/in scope
      (ds/add (make-property-view)))))

(defn get-control
  "This is called by the Java shim GenericPropertySheetPage. Not for other use."
  [property-view-node]
  (-> property-view-node
      ds/refresh
      :widget-tree
      (ui/widget [:form])))

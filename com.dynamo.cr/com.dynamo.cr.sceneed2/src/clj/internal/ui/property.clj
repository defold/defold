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

(defprotocol Presenter
  (control-for-property [this])
  (display-property [this editor value]))

(defrecord StringPresenter []
  Presenter
  (control-for-property [_]
    {:type :text})
  (display-property [_ editor value]
    (ui/apply-properties editor {:text (str value)})))

(defrecord DefaultPresenter []
  Presenter
  (control-for-property [_]
    {:type :label})
  (display-property [_ editor value]
    (ui/apply-properties editor {:text (str value)})))

(defmulti presenter-for-property (comp :value-type :type))
(defmethod presenter-for-property s/Str [_] (->StringPresenter))
(defmethod presenter-for-property :default [_] (->DefaultPresenter))

(defn- property-control-strip
  [[prop-name presenter]]
  (let [label-text (niceify-label prop-name)]
    [[:label-composite {:type :composite
                        :layout {:type :stack}
                        :children [[:label      {:type :label :text label-text}]
                                   [:label-link {:type :hyperlink :text label-text :underlined true :on-click (fn [_] (prn "RESET " prop-name)) :foreground [0 0 255] :tooltip-text Messages/FormPropertySheetViewer_RESET_VALUE}]]}]
     [prop-name (merge {:layout-data {:type :grid :grab-excess-horizontal-space true :horizontal-alignment SWT/FILL :width-hint 50}}
                  (control-for-property presenter))]
     [:dummy           {:type :label :layout-data {:type :grid :exclude true}}]
     [:status-label    {:type :status-label :style :border :status Status/OK_STATUS :layout-data {:type :grid :grab-excess-horizontal-space true :horizontal-fill SWT/HORIZONTAL :width-hint 50 :exclude true}}]]))

(defn- make-property-page-ui
  [toolkit parent presenters]
  (ui/make-control toolkit (ui/widget parent [:form :composite])
    [:page-content
     {:type :composite
      :layout {:type :grid :num-columns 2 :margin-width 0}
      :children (mapcat property-control-strip presenters)}]))

(defn- make-property-page
  [toolkit properties-form properties]
  (let [presenters (map-vals presenter-for-property properties)
        property-page-ui (make-property-page-ui toolkit properties-form presenters)]
    [presenters property-page-ui]))

(defn- make-empty-property-page-ui
  [toolkit parent]
  (ui/make-control toolkit (ui/widget parent [:form :composite])
    [:page-content
     {:type :composite
      :layout {:type :grid}
      :children [[:no-selection-label {:type :label :text Messages/FormPropertySheetViewer_NO_PROPERTIES}]]}]))

(defn- make-empty-property-page
  [toolkit properties-form]
  [[] (make-empty-property-page-ui toolkit properties-form)])

(defn- update-property-page
  [[presenters page-content] properties]
  (doseq [[prop-name {:keys [value type]}] properties
          :let [widget (ui/widget page-content [:page-content prop-name])]]
    (display-property (get presenters prop-name) widget value))
  page-content)

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
  [{:keys [sheet-cache toolkit properties-form] :as node}]
  (let [properties (in/get-node-value node :content)]
    (-> sheet-cache
        (lookup-or-create (cache-key properties) make-property-page toolkit properties-form properties)
        (update-property-page properties)
        (ui/widget [:page-content])
        (ui/bring-to-front)))
  (ui/scroll-to-top (ui/widget properties-form [:form])))

(defn- refresh-after-a-while
  [graph this transaction]
  (when (and (ds/is-modified? transaction this :content) (:debouncer this))
    (t/signal (:debouncer this))))

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
    (let [toolkit         (FormToolkit. (.getDisplay ^Composite (:parent event)))
          properties-form (ui/make-control toolkit (:parent event) gui)
          sheet-cache     (atom {})]
      (lookup-or-create sheet-cache (cache-key {}) make-empty-property-page toolkit properties-form)
      (ds/set-property self
        :sheet-cache     sheet-cache
        :toolkit         toolkit
        :properties-form properties-form
        :debouncer       (ui/display-debouncer 100 #(refresh-property-page (ds/refresh self))))))

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
      :properties-form
      (ui/widget [:form])))

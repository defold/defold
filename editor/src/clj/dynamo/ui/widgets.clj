(ns dynamo.ui.widgets
  (:require [clojure.tools.macro :refer [name-with-attributes]]
            [editor.project :as p]
            [dynamo.ui :as ui]
            [javafx-wrapper :refer [apply-properties]]
            [javafx.scene.control :refer [button color-picker hyperlink label scroll-pane]]
            [javafx.scene.layout :refer [stack-pane grid-pane]]
            [javafx.scene.text :refer [text]]
            [internal.java :refer [map-beaner]]
            [camel-snake-kebab :refer :all])
  (:import [javafx.scene Node]
           [javafx.scene.control Control ColorPicker]
           [javafx.scene.paint Color]
           [javafx.scene.text Text]))

(defmulti make-control (fn [_ [_ {type :type}]] type))

(defn- rgb
  ([s]       (Color/valueOf (if (keyword? s) (name s) (str s))))
  ([r g b]   (Color/rgb r g b))
  ([r g b a] (Color/rgb r g b a)))

(defn bring-to-front!
  [^Control control]
  #_(let [composite ^Composite   (.getParent control)
        layout    ^StackLayout (.getLayout composite)]
    (assert (instance? StackLayout layout) "bring-to-front only works on Composites with StackLayout as their layout")
    (when (not= (. layout topControl) control)
      (set! (. layout topControl) control)
      (.layout composite))))

(defn scroll-to-top!
  [form]
  #_(.setOrigin form 0 0)
  #_(.reflow form true))

(defn- set-widget-data [^Node control key value]
  (.setUserData control (assoc (.getUserData control) key value)))

(defn- get-widget-data [^Node control key]
  (get (.getUserData control) key))

(defn set-user-data [^Node control value]
  (set-widget-data control ::user-data value))

(defn get-user-data [^Node control]
  (get-widget-data control ::user-data))

(defmulti make-layout      (fn [_ {type :type}] type))
(defmulti make-layout-data (fn [_ {type :type}] type))

(defn ^:private property-setters [desiderata control props]
  (for [prop desiderata
        :let [accessor-fn (symbol (name prop))]]
    `(when-let [v# (get ~props ~prop)]
       (~accessor-fn ~control v#))))

#_(def swt-style
  {:none   SWT/NONE
   :border SWT/BORDER})

#_(def swt-horizontal-alignment
  {:left   SWT/BEGINNING
   :center SWT/CENTER
   :right  SWT/END
   :fill   SWT/FILL})

#_(def swt-vertical-alignment
  {:top    SWT/BEGINNING
   :center SWT/CENTER
   :bottom SWT/END
   :fill   SWT/FILL})

#_(def swt-grid-layout (map-beaner GridLayout))
#_(def swt-grid-data   (map-beaner GridData))

(defmethod make-layout :grid
  [control spec]
  (let [columns (:columns spec)
        spec (assoc spec :num-columns (count columns))]
    nil))

#_(defmethod make-layout-data :grid
  [control spec]
  (let [parent (.getParent control)
        children (vec (.getChildren parent))
        columns (:columns (get-widget-data parent ::layout))
        ; assume no colspan/rowspan
        child-index (.indexOf children control)
        column-index (mod child-index (count columns))
        column-layout (nth columns column-index)
        spec (merge column-layout spec)
        defaults (merge {}
                   (when (= :fill (:horizontal-alignment spec)) {:grab-excess-horizontal-space true})
                   (when (= :fill (:vertical-alignment   spec)) {:grab-excess-vertical-space   true}))
        spec (into defaults
               (for [[k v] spec]
                (case k
                  :horizontal-alignment [k (swt-horizontal-alignment v)]
                  :vertical-alignment   [k (swt-vertical-alignment   v)]
                  :min-width  [:width-hint  v]
                  :min-height [:height-hint v]
                  [k v])))]
    (swt-grid-data spec)))


(defmethod make-control :form
  [_ [name props :as spec]]
  (let [scroll-pane    (scroll-pane)
        body           (grid-pane)
        _              (set-widget-data body ::layout (:layout props))
        child-controls (reduce merge {} (map #(make-control nil body %) (:children props)))]
    (apply-properties scroll-pane (assoc props :content body))
    {name (merge child-controls {::widget scroll-pane})}))

(defmethod make-control :stack
  [_ [name props :as spec]]
  (let [child-controls (reduce merge {} (map #(make-control nil nil %) (:children props)))
        stack          (stack-pane child-controls)]
    (apply-properties stack props)
    {name (merge child-controls {::widget stack})}))

(defmethod make-control :grid
  [_ [name props :as spec]]
  (let [child-controls (reduce merge {} (map #(make-control nil nil %) (:children props)))
        grid           (grid-pane)]
    ;; TODO - handle adding child controls and setting grid constraints based on their layout info
    (apply-properties grid props)
    {name (merge child-controls {::widget grid})}))

(defmethod make-control :label
  [_ [name props :as spec]]
  (let [control (label)]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :hyperlink
  [_ [name props :as spec]]
  (let [control (hyperlink)]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :text
  [_ [name props :as spec]]
  (let [control (text)]
    (apply-properties control props)
    {name {::widget control}}))

#_(defmethod make-control :status-label
  [_ parent [name props :as spec]]
  (let [control (StatusLabel. parent (swt-style (:style props :none)))]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :color-selector
  [_ [name props :as spec]]
  (let [control (color-picker)]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :button
  [_ [name props :as spec]]
  (let [control (button)]
    (apply-properties control props)
    {name {::widget control}}))

;; TODO - resurrect resource picker
#_(defmethod make-control :resource-selector
  [_ [name props :as spec]]
  (let [project-node       (get-in props [:user-data :project-node])
        selection-callback (fn [] (p/select-resources project-node (:extensions props) (:title props)))
        control            (ResourceSelector.)]
    (apply-properties control props)
    {name {::widget control}}))

(defn widget
  [widget-tree path]
  (get-in widget-tree (concat path [::widget])))

(defn update-ui! [widget-subtree settings]
  (apply-properties (widget widget-subtree []) (dissoc settings :children))
  (doseq [[child-name child-settings] (:children settings)]
    (when-let [widget (get widget-subtree child-name)]
      (update-ui! widget child-settings))))

(defn get-text [^Text widget]
  (.getText widget))

(defn get-color [^ColorPicker widget]
  (let [c (.getValue widget)]
    [(.-red c) (.-green c) (.-blue c)]))

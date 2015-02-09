(ns dynamo.ui.widgets
  (:require [dynamo.project :as p]
            [dynamo.ui :as ui]
            [internal.java :refer [map-beaner]])
  (:import [org.eclipse.swt SWT]
           [org.eclipse.swt.custom StackLayout]
           [org.eclipse.swt.graphics Color RGB]
           [org.eclipse.swt.layout GridData GridLayout]
           [org.eclipse.swt.widgets Button Control Composite Label Text Widget]
           [org.eclipse.ui.forms.events HyperlinkAdapter]
           [org.eclipse.ui.forms.widgets FormToolkit Hyperlink ScrolledForm]
           [com.dynamo.cr.properties StatusLabel]
           [internal.ui ColorSelector ResourceSelector]))

(set! *warn-on-reflection* true)

(defn- rgb [r g b] (Color. (ui/display) r g b))

(defn bring-to-front!
  [^Control control]
  (let [composite ^Composite   (.getParent control)
        layout    ^StackLayout (.getLayout composite)]
    (assert (instance? StackLayout layout) "bring-to-front only works on Composites with StackLayout as their layout")
    (when (not= (. layout topControl) control)
      (set! (. layout topControl) control)
      (.layout composite))))

(defn scroll-to-top!
  [^ScrolledForm form]
  (.setOrigin form 0 0)
  (.reflow form true))


(defn- set-widget-data [^Widget control key value]
  (.setData control (assoc (.getData control) key value)))

(defn- get-widget-data [^Widget control key]
  (get (.getData control) key))

(defn set-user-data [^Widget control value]
  (set-widget-data control ::user-data value))

(defn get-user-data [^Widget control]
  (get-widget-data control ::user-data))

(defmulti make-layout      (fn [_ {type :type}] type))
(defmulti make-layout-data (fn [_ {type :type}] type))
(defmulti make-control     (fn [_ _ [_ {type :type}]] type))

(defmacro ^:private gen-state-changes [desiderata control props]
  (let [m {:background   `(when-let [v# (:background ~props)]       (.setBackground ~control (apply rgb v#)))
           :echo-char    `(when-let [v# (:echo-char ~props)]        (.setEchoChar ~control v#))
           :editable     `(when-let [v# (:editable ~props)]         (.setEditable ~control v#))
           :foreground   `(when-let [v# (:foreground ~props)]       (.setForeground ~control (apply rgb v#)))
           :on-click     `(when-let [v# (:on-click ~props)]         (.addHyperlinkListener ~control (proxy [HyperlinkAdapter] [] (linkActivated [e#] (v# e#)))))
           :layout       `(when-let [v# (:layout ~props)]           (.setLayout ~control (make-layout ~control v#)))
           :layout-data  `(let [v# (:layout-data ~props)]
                            (when-let [parent-type# (-> (.getParent ~control) (get-widget-data ::layout) :type)]
                              (.setLayoutData ~control (make-layout-data ~control (assoc v# :type parent-type#)))
                              (.layout (.getParent ~control))))
           :listen       `(doseq [[e# t#] (:listen ~props)]         (.addListener ~control (ui/event-map e#) t#))
           :status       `(when-let [v# (:status ~props)]           (.setStatus ~control v#))
           :text         `(when-let [v# (:text ~props)]             (.setText ~control v#))
           :text-limit   `(when-let [v# (:text-limit ~props)]       (.setTextLimit ~control v#))
           :top-index    `(when-let [v# (:top-index ~props)]        (.setTopIndex ~control v#))
           :tooltip-text `(when-let [v# (:tooltip-text ~props)]     (.setToolTipText ~control v#))
           :underlined   `(when-let [v# (:underlined ~props)]       (.setUnderlined ~control v#))
           :user-data    `(when-let [v# (:user-data ~props)]        (set-user-data ~control v#))
           :visible      `(when-not (nil? (:visible ~props))        (.setVisible ~control (:visible ~props)))}]
    `(do ~@(vals (select-keys m desiderata)))))

(defprotocol Mutable
  (apply-properties [this props]))

(extend-protocol Mutable
  ScrolledForm
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :visible :tooltip-text :layout-data :listen :user-data} this props)
    (gen-state-changes #{:layout} (.getBody this) props)
    this)

  Composite
  (apply-properties [this props]
    (gen-state-changes #{:foreground :background :visible :layout :layout-data :tooltip-text :listen :user-data} this props)
    this)

  ColorSelector
  (apply-properties [this props]
    (gen-state-changes #{:layout-data :listen :user-data :visible} this props)
    (gen-state-changes #{:foreground :tooltip-text :text} (.getButton this) props)
    (when-let [[^int r ^int g ^int b] (:color props)] (.setColorValue this (RGB. r g b)))
    this)

  Hyperlink
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :visible :on-click :layout-data :underlined :tooltip-text :listen :user-data} this props)
    this)

  Label
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :visible :layout-data :tooltip-text :listen :user-data} this props)
    this)

  Text
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :visible :layout-data :tooltip-text :echo-char :text-limit :top-index :editable :listen :user-data} this props)
    this)

  StatusLabel
  (apply-properties [this props]
    (gen-state-changes #{:status :foreground :background :visible :layout-data :tooltip-text :listen :user-data} this props)
    this)

  Button
  (apply-properties [this props]
    (gen-state-changes #{:text :layout-data :tooltip-text :listen :user-data :visible} this props)
    this)

  ResourceSelector
  (apply-properties [this props]
    (gen-state-changes #{:layout-data :listen :user-data :visible} this props)
    (gen-state-changes #{:foreground :tooltip-text :text} (.getButton this) props)
    this))

(def swt-style
  {:none   SWT/NONE
   :border SWT/BORDER})

(def swt-horizontal-alignment
  {:left   SWT/BEGINNING
   :center SWT/CENTER
   :right  SWT/END
   :fill   SWT/FILL})

(def swt-vertical-alignment
  {:top    SWT/BEGINNING
   :center SWT/CENTER
   :bottom SWT/END
   :fill   SWT/FILL})

(def swt-grid-layout (map-beaner GridLayout))
(def swt-grid-data   (map-beaner GridData))

(defmethod make-layout :stack
  [^Composite control _]
  (let [stack (StackLayout.)]
    (set! (. stack topControl) (first (.getChildren control)))
    stack))

(defmethod make-layout-data :stack [^Control control _]
  nil)

(defmethod make-layout :grid [^Composite control spec]
  (let [columns (:columns spec)
        spec (assoc spec :num-columns (count columns))]
    (swt-grid-layout spec)))

(defmethod make-layout-data :grid [^Control control spec]
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
  [^FormToolkit toolkit parent [name props :as spec]]
  (let [control ^ScrolledForm (.createScrolledForm toolkit parent)
        body (.getBody control)
        _ (set-widget-data body ::layout (:layout props))
        child-controls (reduce merge {} (map #(make-control toolkit body %) (:children props)))]
    (apply-properties control props)
    {name (merge child-controls {::widget control})}))

(defmethod make-control :composite
  [^FormToolkit toolkit parent [name props :as spec]]
  (let [control (.createComposite toolkit parent (swt-style (:style props :none)))
        _ (set-widget-data control ::layout (:layout props))
        child-controls (reduce merge {} (map #(make-control toolkit control %) (:children props)))]
    (apply-properties control props)
    {name (merge child-controls {::widget control})}))

(defmethod make-control :label
  [^FormToolkit toolkit parent [name props :as spec]]
  (let [control (.createLabel toolkit parent nil (swt-style (:style props :none)))]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :hyperlink
  [^FormToolkit toolkit parent [name props :as spec]]
  (let [control (.createHyperlink toolkit parent nil (swt-style (:style props :none)))]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :text
  [^FormToolkit toolkit parent [name props :as spec]]
  (let [control (.createText toolkit parent nil (swt-style (:style props :none)))]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :status-label
  [_ parent [name props :as spec]]
  (let [control (StatusLabel. parent (swt-style (:style props :none)))]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :color-selector
  [^FormToolkit toolkit parent [name props :as spec]]
  (let [control (ColorSelector. toolkit parent (swt-style (:style props :none)))]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :button
  [^FormToolkit toolkit parent [name props :as spec]]
  (let [control (.createButton toolkit parent nil (swt-style (:style props :none)))]
    (apply-properties control props)
    {name {::widget control}}))

(defmethod make-control :resource-selector
  [^FormToolkit toolkit parent [name props :as spec]]
  #_"ResourceSelect widget is rendered as a browse button.
  Click this button to reveal a modal resource selection dialog; select a resource and click \"OK\".
  This will fire a `:selection` event; the selection (seq of ResourceNode) is accessible in the event map under the `:data` key.
  No events are fired when the user clicks \"Cancel\", presses Esc, or otherwise dismisses the dialog without making a selection."
  (let [project-node       (get-in props [:user-data :project-node])
        selection-callback (fn [] (p/select-resources project-node (:extensions props) (:title props)))
        control            (ResourceSelector. toolkit parent (swt-style (:style props :none)) selection-callback)]
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

(defn get-color [^ColorSelector widget]
  (let [c (.getColorValue widget)]
    [(.-red c) (.-green c) (.-blue c)]))

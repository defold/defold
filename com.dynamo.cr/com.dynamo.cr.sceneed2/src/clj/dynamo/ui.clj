(ns dynamo.ui
  (:require [dynamo.types :as t]
            [internal.ui.handlers :as h]
            [internal.java :refer [bean-mapper map-beaner]]
            [camel-snake-kebab :refer :all]
            [service.log :as log])
  (:import [com.dynamo.cr.sceneed.core SceneUtil SceneUtil$MouseType]
           [org.eclipse.core.runtime SafeRunner]
           [org.eclipse.jface.util SafeRunnable]
           [org.eclipse.swt.custom StackLayout]
           [org.eclipse.swt.widgets Control Composite Display Event Label Listener Shell Text Widget]
           [org.eclipse.swt.graphics Color RGB]
           [org.eclipse.ui.forms.widgets FormToolkit Hyperlink ScrolledForm]
           [org.eclipse.ui.forms.events HyperlinkAdapter HyperlinkEvent]
           [org.eclipse.swt.layout FillLayout GridData GridLayout]
           [org.eclipse.swt SWT]
           [com.dynamo.cr.properties StatusLabel]
           [internal.ui ColorSelector]))

(set! *warn-on-reflection* true)

(defn mouse-type
  []
  (if (= SceneUtil$MouseType/ONE_BUTTON (SceneUtil/getMouseType))
    :one-button
    :three-button))

(defn ^Display display
  []
  (or (Display/getCurrent) (Display/getDefault)))

(defn- is-display-thread?
  [^Display display]
  (= (.getThread display) (Thread/currentThread)))

(defn swt-thread-safe*
  [f]
  (.asyncExec (display) f))

(defmacro swt-safe
  [& body]
  `(swt-thread-safe* (bound-fn [] ~@body)))

(defn swt-await*
  [f]
  (let [res (promise)]
    (.syncExec (display)
      #(deliver res (f)))
    (deref res)))

(defmacro swt-await
  [& body]
  `(swt-await* (bound-fn [] ~@body)))

(defn swt-timed-exec*
  [after f]
  (when-let [d (display)]
    (if (is-display-thread? d)
      (.timerExec d after f)
      (swt-thread-safe* #(.timerExec d after f)))))

(defmacro after
  [wait-time & body]
  `(swt-timed-exec* ~wait-time (bound-fn [] ~@body)))

(defn tick
  [interval f]
  (if-let [d (display)]
    (letfn [(tickf [] (f) (.timerExec d interval tickf))]
      (.asyncExec d tickf)
      tickf)
    (log/error :message "Display is not available.")))

(defn untick
  [tickf]
  (swt-thread-safe* #(.timerExec (display) -1 tickf)))

(defmacro swt-events [& evts]
  (let [keywords (map (comp keyword ->kebab-case str) evts)
        constants (map symbol (map #(str "SWT/" %) evts))]
    (zipmap keywords constants)))

(defmacro run-safe
  [& body]
  `(SafeRunner/run
     (proxy [SafeRunnable] []
       (run []
         ~@body))))

(def event-map (swt-events Dispose Resize Paint MouseDown MouseUp MouseDoubleClick
                           MouseEnter MouseExit MouseHover MouseMove MouseWheel DragDetect
                           FocusIn FocusOut Gesture KeyUp KeyDown MenuDetect Traverse
                           Selection DefaultSelection))

(def event-type-map (clojure.set/map-invert event-map))

(defn event-type [^Event evt]
  (event-type-map (.type evt)))

(def ^:private e->m (bean-mapper Event))

(defn event->map
  [evt]
  (update-in (e->m evt) [:type] event-type-map))

(defn make-listener [callback-fn args]
  (proxy [Listener] []
    (handleEvent [evt]
      (apply callback-fn (event->map evt) args))))

(defn listen
  [^Widget component type callback-fn & args]
  (.addListener component (event-map type)
    (make-listener callback-fn args)))

(defprotocol EventRegistration
  (add-listener [this key listener])
  (remove-listener [this key]))

(defprotocol EventSource
  (send-event [this event]))

(defrecord EventBroadcaster [listeners]
  EventRegistration
  (add-listener [this key listener] (swap! listeners assoc key listener))
  (remove-listener [this key] (swap! listeners dissoc key))

  EventSource
  (send-event [this event]
    (swt-await
      (doseq [l (vals @listeners)]
       (run-safe
         (l event))))))

(defn make-event-broadcaster [] (EventBroadcaster. (atom {})))

(defn- rgb [r g b] (Color. (display) r g b))


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
                              (.setLayoutData ~control (make-layout-data ~control (assoc v# :type parent-type#)))))
           :listen       `(doseq [[e# t#] (:listen ~props)]         (.addListener ~control (event-map e#) t#))
           :status       `(when-let [v# (:status ~props)]           (.setStatus ~control v#))
           :text         `(when-let [v# (:text ~props)]             (.setText ~control v#))
           :text-limit   `(when-let [v# (:text-limit ~props)]       (.setTextLimit ~control v#))
           :top-index    `(when-let [v# (:top-index ~props)]        (.setTopIndex ~control v#))
           :tooltip-text `(when-let [v# (:tooltip-text ~props)]     (.setToolTipText ~control v#))
           :underlined   `(when-let [v# (:underlined ~props)]       (.setUnderlined ~control v#))
           :user-data    `(when-let [v# (:user-data ~props)]        (set-user-data ~control v#))}]
    `(do ~@(vals (select-keys m desiderata)))))

(defprotocol Mutable
  (apply-properties [this props]))

(extend-protocol Mutable
  ScrolledForm
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :tooltip-text :layout-data :listen :user-data} this props)
    (gen-state-changes #{:layout} (.getBody this) props)
    this)

  Composite
  (apply-properties [this props]
    (gen-state-changes #{:foreground :background :layout :layout-data :tooltip-text :listen :user-data} this props)
    this)

  ColorSelector
  (apply-properties [this props]
    (gen-state-changes #{:layout-data :listen :user-data} this props)
    (gen-state-changes #{:foreground :tooltip-text :text} (.getButton this) props)
    (when-let [[^int r ^int g ^int b] (:color props)] (.setColorValue this (RGB. r g b)))
    this)

  Hyperlink
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :on-click :layout-data :underlined :tooltip-text :listen :user-data} this props)
    this)

  Label
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :layout-data :tooltip-text :listen :user-data} this props)
    this)

  Text
  (apply-properties [this props]
    (gen-state-changes #{:text :foreground :background :layout-data :tooltip-text :echo-char :text-limit :top-index :editable :listen :user-data} this props)
    this)

  StatusLabel
  (apply-properties [this props]
    (gen-state-changes #{:status :foreground :background :layout-data :tooltip-text :listen :user-data} this props)
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

(defn shell [] (doto (Shell.) (.setLayout (FillLayout.))))

(defn bring-to-front!
  [widget-tree]
  (let [control   ^Control     (widget widget-tree [])
        composite ^Composite   (.getParent control)
        layout    ^StackLayout (.getLayout composite)]
    (assert (instance? StackLayout layout) "bring-to-front only works on Composites with StackLayout as their layout")
    (when (not= (. layout topControl) control)
      (set! (. layout topControl) control)
      (.layout composite))))

(defn scroll-to-top!
  [widget-tree]
  (let [form ^ScrolledForm (widget widget-tree [])]
    (.setOrigin form 0 0)
    (.reflow form true)))

(defn now [] (System/currentTimeMillis))

(deftype DisplayDebouncer [delay action ^:volatile-mutable signaled ^:volatile-mutable last-signaled]
  t/Condition
  (signal [this]
    (set! last-signaled (now))
    (set! signaled true)
    (.asyncExec (display) this))

  t/Cancelable
  (cancel [this]
    (set! signaled false))

  Runnable
  (run [this]
    (when signaled
      (if (< delay (- (now) last-signaled))
        (do
          (action)
          (set! signaled false))
        (.asyncExec (display) this)))))

(defn display-debouncer
  [when f]
  (->DisplayDebouncer when f nil nil))

(defmacro defcommand
  [name category-id command-id label]
  `(def ^:command ~name (h/make-command ~label ~category-id ~command-id)))

(defmacro defhandler
  [name command & body]
  (let [enablement (if (= :enabled-when (first body)) (second body) nil)
        body       (if (= :enabled-when (first body)) (drop 2 body) body)
        fn-var     (first body)
        body       (rest body)]
    `(def ~name (h/make-handler ~command ~fn-var ~@body))))

(doseq [[v doc]
        {*ns*
         "Interaction with the development environment itself.

This namespace has the functions and macros you use to write
tools in the editor."

         #'defcommand
         "Create a command with the given category and id. Binds
the resulting command to the named variable.

Label should be a human-readable string. It will appear
directly in the UI (unless there is a translation for it.)

If you use the same category-id and command-id more than once,
this will create independent entities that refer to the same underlying
command."

         #'defhandler
         "Creates a handler and binds it to the given command.

In the first form, the handler will always be enabled. Upon invocation, it
will call the function bound to fn-var with the
org.eclipse.core.commands.ExecutionEvent and the additional args.

In the second form, enablement-fn will be checked. When it returns a truthy
value, the handler will be enabled. Enablement-fn must have metadata to
identify the evaluation context variables and properties that affect its
return value."}]
  (alter-meta! v assoc :doc doc))

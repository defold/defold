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

(ns editor.ui
  (:require [cljfx.fx.image-view :as fx.image-view]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [clojure.xml :as xml]
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.handler :as handler]
            [editor.icons :as icons]
            [editor.keymap :as keymap]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.os :as os]
            [editor.progress :as progress]
            [editor.system :as system]
            [internal.util :as util]
            [service.log :as log]
            [service.smoke-log :as slog]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.eduction :as e]
            [util.profiler :as profiler])
  (:import [com.defold.control ListCell]
           [com.defold.control LongField]
           [com.defold.control DefoldStringConverter TreeCell]
           [com.sun.javafx.event DirectEvent]
           [com.sun.javafx.scene NodeHelper]
           [java.awt Desktop Desktop$Action]
           [java.io File IOException]
           [java.net URI]
           [java.util Collection]
           [javafx.animation AnimationTimer KeyFrame KeyValue Timeline]
           [javafx.application Platform]
           [javafx.beans InvalidationListener Observable]
           [javafx.beans.binding Bindings]
           [javafx.beans.property ReadOnlyProperty]
           [javafx.beans.value ChangeListener ObservableValue]
           [javafx.collections FXCollections ListChangeListener ObservableList]
           [javafx.css Styleable]
           [javafx.event ActionEvent Event EventDispatcher EventHandler EventTarget]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Orientation Point2D]
           [javafx.scene Cursor Group Node Parent Scene]
           [javafx.scene.control Button ButtonBase Cell CheckBox CheckMenuItem ChoiceBox ColorPicker ComboBox ComboBoxBase ContextMenu Control Label Labeled ListView Menu MenuBar MenuButton MenuItem MultipleSelectionModel ProgressBar SelectionMode SelectionModel Separator SeparatorMenuItem Tab TabPane TableView TextArea TextField TextInputControl Toggle ToggleButton Tooltip TreeItem TreeTableView TreeView]
           [javafx.scene.image Image ImageView]
           [javafx.scene.input Clipboard ContextMenuEvent DragEvent KeyCode KeyCombination KeyEvent MouseButton MouseEvent]
           [javafx.scene.layout AnchorPane GridPane HBox Pane Priority]
           [javafx.scene.shape SVGPath]
           [javafx.stage Modality PopupWindow Stage StageStyle Window]
           [javafx.util Callback Duration]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private ^:dynamic *programmatic-selection* nil)

(defonce ^:dynamic *main-stage* (atom nil))

;; Slight hack to work around the fact that we have not yet found a
;; reliable way of detecting when the application loses focus.

;; If no application-owned windows have had focus within this
;; threshold, we consider the application to have lost focus.
(defonce ^:private ^:const application-unfocused-threshold-ms 500)
(defonce ^:private focus-state (atom nil))
(defonce ^:private application-unfocused-tasks (atom {}))
(defonce ^:private stopped-timers (atom #{}))

(declare ->future)

(defn- cancel-application-unfocused-task! [key]
  (when-let [{:keys [timer]} (get @application-unfocused-tasks key)]
    (.stop ^Timeline timer)
    (swap! application-unfocused-tasks dissoc key))
  nil)

(defn- schedule-application-unfocused-task! [key timestamp application-unfocused! args]
  (cancel-application-unfocused-task! key)
  (let [delay (/ (double application-unfocused-threshold-ms) 1000.0)
        timer (->future delay
                        (fn []
                          (let [{stored-timestamp :timestamp} (get @application-unfocused-tasks key)]
                            (when (= stored-timestamp timestamp)
                              (swap! application-unfocused-tasks dissoc key)
                              (apply application-unfocused! args)))))]
    (swap! application-unfocused-tasks assoc key {:timer timer :timestamp timestamp})
    nil))

(defn node? [value]
  (instance? Node value))

(defn- set-application-focus-state [old-state focused window t]
  ;; How we expect the focus to be changed over time (this works on e.g. macOS):
  ;; [user opens dialog...]
  ;; 1. main window: focus loss
  ;; 2. dialog window: focus gain
  ;; [...user interacts with dialog, then closes it...]
  ;; 3. dialog window: focus loss
  ;; 4. main window: focus gain
  ;; How the focus actually works on Linux:
  ;; [user opens dialog...]
  ;; 1. dialog window: focus gain
  ;; 2. main window: focus loss
  ;; [...user interacts with dialog, then closes it...]
  ;; 3. main window: focus gain
  ;; When the dialog is open for longer than `application-unfocused-threshold-ms`,
  ;; this Linux behavior causes resource sync to trigger after closing the dialog,
  ;; which may lead to subtle bugs if the normal use of the dialog also triggers
  ;; the resource sync. To fix the issue on Linux, we skip focus changes that
  ;; report focus loss while another window is currently focused
  (if (and (:focused old-state)
           (not focused)
           (not= (:window old-state) window))
    old-state
    {:focused focused :window window :t t}))

(def focus-change-listener
  (reify ChangeListener
    (changed [_ observable-value _ focused]
      (let [window (.getBean ^ReadOnlyProperty observable-value)
            t (System/currentTimeMillis)]
        (swap! focus-state set-application-focus-state focused window t)))))

(defn add-application-focused-callback! [key application-focused! & args]
  (add-watch focus-state key
             (fn [_key _ref old new]
               (when (and old
                          (not (:focused old))
                          (:focused new))
                 (let [^long new-t (:t new)
                       ^long old-t (:t old)
                       unfocused-ms (- new-t old-t)]
                   (when (< application-unfocused-threshold-ms unfocused-ms)
                     (apply application-focused! args))))))
  nil)

(defn remove-application-focused-callback! [key]
  (remove-watch focus-state key)
  nil)

(defn add-application-unfocused-callback! [key application-unfocused! & args]
  (add-watch focus-state key
             (fn [_key _ref old new]
               (when (:focused new)
                 (cancel-application-unfocused-task! key))
               (when (and old
                          (:focused old)
                          (not (:focused new)))
                 (schedule-application-unfocused-task! key (:t new) application-unfocused! args))))
  nil)

(defn remove-application-unfocused-callback! [key]
  (cancel-application-unfocused-task! key)
  (remove-watch focus-state key)
  nil)

(defonce/protocol Text
  (text ^String [this])
  (text! [this ^String val]))

(defonce/protocol HasAction
  (on-action! [this fn]))

(defonce/protocol Cancellable
  (on-cancel! [this cancel-fn]))

(defonce/protocol HasValue
  (value [this])
  (value! [this val]))

(defonce/protocol HasUserData
  (user-data [this key])
  (user-data! [this key val]))

(defonce/protocol Editable
  (editable [this])
  (editable! [this val])
  (on-edit! [this fn]))

(defonce/protocol HasSelectionModel
  (^SelectionModel selection-model [this]))

(def application-icon-image (with-open [in (io/input-stream (io/resource "logo_blue.png"))]
                              (Image. in)))

; NOTE: This one might change from welcome to actual project window
(defn set-main-stage [main-stage]
  (reset! *main-stage* main-stage))

(defn main-stage ^Stage []
  @*main-stage*)

(defn main-scene ^Scene []
  (.. (main-stage) (getScene)))

(defn main-root ^Node []
  (.. (main-scene) (getRoot)))

(defn- main-menu-id ^MenuBar []
  (:menu-id (user-data (main-root) ::menubar)))

(defn closest-node-where
  ^Node [pred ^Node leaf-node]
  (cond
    (nil? leaf-node) nil
    (pred leaf-node) leaf-node
    :else (recur pred (.getParent leaf-node))))

(defn closest-node-of-type
  ^Node [^Class node-type ^Node leaf-node]
  (closest-node-where (partial instance? node-type) leaf-node))

(defn closest-node-with-style
  ^Node [^String style-class ^Node leaf-node]
  (closest-node-where (fn [^Node node]
                        (.contains (.getStyleClass node) style-class))
                      leaf-node))

(defn nodes-along-path?
  "Returns true if it is possible to trace a path from the leaf node up to (but
  not including) the root that includes needle. Also true if needle = leaf."
  [leaf needle root]
  (->> leaf
       (iterate #(.getParent ^Node %))
       (take-while #(not (identical? root %)))
       (some #(identical? needle %))
       boolean))

(defn make-stage
  ^Stage []
  (let [stage (Stage.)]
    ;; This is used to keep track of focus changes, and must be installed on
    ;; every Stage created by our application so we can detect when it regains
    ;; focus from an external process.
    (.addListener (.focusedProperty stage) ^ChangeListener focus-change-listener)

    ;; We don't want icons in the title bar on macOS. The application bundle
    ;; provides the .app icon when bundling and child windows are rendered
    ;; as miniatures when minimized, so there is no need to assign an icon
    ;; to each window on macOS unless we want the icon in the title bar.
    (when-not (os/is-mac-os?)
      (.. stage getIcons (add application-icon-image)))
    stage))

(defn make-dialog-stage
  (^Stage []
   ;; If a main stage exists, try to make our dialog stage window-modal to it.
   ;; Otherwise fall back on an application-modal dialog. This is not preferred
   ;; on macOS as maximizing an ownerless window will enter full-screen mode.
   ;; TODO: Find a way to block application-modal dialogs from full-screen mode.
   (make-dialog-stage (main-stage)))
  (^Stage [^Window owner]
   (let [stage (make-stage)]
     (.initStyle stage StageStyle/DECORATED)
     (.setResizable stage false)
     (if (nil? owner)
       (.initModality stage Modality/APPLICATION_MODAL)
       (do (.initModality stage Modality/WINDOW_MODAL)
           (.initOwner stage owner)))
     stage)))

(defn collect-controls [^Parent root keys]
  (let [controls (zipmap (map keyword keys) (map #(.lookup root (str "#" %)) keys))
        missing (->> controls
                  (filter (fn [[k v]] (when (nil? v) k)))
                  (map first))]
    (when (seq missing)
      (throw (Exception. (format "controls %s are missing" (string/join ", " (map (comp str name) missing))))))
    controls))

(defmacro with-controls [parent child-syms & body]
  (let [child-keys (map keyword child-syms)
        child-ids (mapv str child-syms)
        all-controls-sym (gensym)
        assignment-pairs (map #(vector %1 (list %2 all-controls-sym)) child-syms child-keys)]
    `(let [~all-controls-sym (collect-controls ~parent ~child-ids)
           ~@(mapcat identity assignment-pairs)]
       ~@body)))

; TODO: Better name?
(defn fill-control [control]
  (AnchorPane/setTopAnchor control 0.0)
  (AnchorPane/setBottomAnchor control 0.0)
  (AnchorPane/setLeftAnchor control 0.0)
  (AnchorPane/setRightAnchor control 0.0))

(defn local-bounds [^Node node]
  (let [b (.getBoundsInLocal node)]
    {:minx (.getMinX b)
     :miny (.getMinY b)
     :maxx (.getMaxX b)
     :maxy (.getMaxY b)
     :width (.getWidth b)
     :height (.getHeight b)}))

(defn node-array
  ^"[Ljavafx.scene.Node;" [nodes]
  (into-array Node nodes))

(defn observable-list
  ^ObservableList [^Collection items]
  (if (empty? items)
    (FXCollections/emptyObservableList)
    (doto (FXCollections/observableArrayList)
      (.addAll items))))

(defn observe [^ObservableValue observable listen-fn]
  (.addListener observable (reify ChangeListener
                             (changed [this observable old new]
                               (listen-fn observable old new)))))

(defn observe-once [^ObservableValue observable listen-fn]
  (let [listener-atom (atom nil)
        listener (reify ChangeListener
                   (changed [_this observable old new]
                     (when-let [^ChangeListener listener @listener-atom]
                       (.removeListener observable listener)
                       (reset! listener-atom nil))
                     (listen-fn observable old new)))]
    (.addListener observable listener)
    (reset! listener-atom listener)
    nil))

(defn observe-list
  ([^ObservableList observable listen-fn]
   (observe-list nil observable listen-fn))
  ([^Node node ^ObservableList observable listen-fn]
   (let [listener (reify ListChangeListener
                    (onChanged [this change]
                      (listen-fn observable (into [] (.getList change)))))]
     (.addListener observable listener)
     (when node
       (let [listeners (-> (or (user-data node ::list-listeners) [])
                           (conj listener))]
         (user-data! node ::list-listeners listeners))))))

(defn remove-list-observers
  [^Node node ^ObservableList observable]
  (let [listeners (user-data node ::list-listeners)]
    (user-data! node ::list-listeners [])
    (doseq [^ListChangeListener l listeners]
      (.removeListener observable l))))

(defn on-ui-thread? []
  (Platform/isFxApplicationThread))

(defn do-run-later [f]
  (Platform/runLater f))

(defn do-run-now [f]
  (if (on-ui-thread?)
    (f)
    (let [p (promise)]
      (do-run-later
        (fn []
          (try
            (deliver p (f))
            (catch Throwable e
              (deliver p e)))))
      (let [val @p]
        (if (instance? Throwable val)
          (throw val)
          val)))))

(defmacro run-now
  [& body]
  `(do-run-now
     (fn [] ~@body)))

(defmacro run-later
  [& body]
  `(do-run-later (fn [] ~@body)))

(defn send-event! [^EventTarget event-target ^Event event]
  (Event/fireEvent event-target (DirectEvent. (.copyFor event event-target event-target))))

(defmacro event-handler [event & body]
  `(reify EventHandler
     (handle [~'this ~event]
       ~@body)))

(defmacro event-dispatcher [event tail & body]
  `(reify EventDispatcher
     (dispatchEvent [~'this ~event ~tail]
       ~@body)))

(def null-event-dispatcher (event-dispatcher event tail))

(defn kill-event-dispatch! [^Node target]
  (.setEventDispatcher target null-event-dispatcher))

(defmacro change-listener [observable old-val new-val & body]
  `(reify ChangeListener
     (changed [~'this ~observable ~old-val ~new-val]
       ~@body)))

(defmacro invalidation-listener [observable & body]
  `(reify InvalidationListener
     (invalidated [~'this ~observable]
       ~@body)))

(defn scene
  ^Scene [^Node node]
  (.getScene node))

(defn add-styles! [^Styleable node classes]
  (let [styles (.getStyleClass node)
        existing (into #{} styles)
        new (filter (complement existing) classes)]
    (when-not (empty? new)
      (.addAll styles ^Collection new))))

(defn add-style! [^Styleable node ^String class]
  (add-styles! node [class]))

(defn remove-styles! [^Styleable node ^Collection classes]
  (let [styles (.getStyleClass node)
        existing (into #{} styles)
        old (filter existing classes)]
    (when-not (empty? old)
      (.removeAll styles ^Collection old))))

(defn remove-style! [^Styleable node ^String class]
  (remove-styles! node [class]))

(defn set-style! [^Styleable node ^String class enabled]
  (let [styles (.getStyleClass node)]
    (if (some (partial = class) styles)
      (when (not enabled)
        (.remove styles class))
      (when enabled
        (.add styles class)))
    nil))

(defn update-tree-cell-style! [^TreeCell cell]
  (let [tree-view (.getTreeView cell)
        expanded-count (.getExpandedItemCount tree-view)
        last-index (- expanded-count 1)
        index (.getIndex cell)]
    (remove-styles! cell ["first-tree-item" "last-tree-item"])
    (when (not (.isEmpty cell))
      (add-styles! cell (remove nil? [(when (= index 0) "first-tree-item")
                                      (when (= index last-index) "last-tree-item")])))))

(defn update-list-cell-style! [^ListCell cell]
  (let [list-view (.getListView cell)
        count (.size (.getItems list-view))
        last-index (- count 1)
        index (.getIndex cell)]
    (remove-styles! cell ["first-list-item" "last-list-item"])
    (when (not (.isEmpty cell))
      (add-styles! cell (remove nil? [(when (= index 0) "first-list-item")
                                      (when (= index last-index) "last-list-item")])))))

(defn cell-item-under-mouse [^MouseEvent event]
  (when-some [^Cell cell (closest-node-of-type Cell (.getTarget event))]
    (.getItem cell)))

(defn max-list-view-cell-width
  "Measure the items in the list view and return the width of
  the widest item, or nil if there are no items in the view."
  [^com.defold.control.ListView list-view]
  (when-some [items (some-> list-view .getItems not-empty vec)]
    (let [sample-cell (doto ^ListCell (.call (.getCellFactory list-view) list-view)
                        (.updateListView list-view))]
      (reduce-kv (fn [^double max-width index item]
                   (.setItem sample-cell item)
                   (.updateIndex sample-cell index)
                   (if (or (some? (.getGraphic sample-cell))
                           (not-empty (.getText sample-cell)))
                     (do (.. list-view getChildren (add sample-cell))
                         (.applyCss sample-cell)
                         (let [width (.prefWidth sample-cell -1)]
                           (.. list-view getChildren (remove sample-cell))
                           (max width max-width)))
                     max-width))
                 0.0
                 items))))

(defn reload-root-styles! []
  (when-let [scene (.getScene ^Stage (main-stage))]
    (let [root ^Parent (.getRoot scene)
          styles (vec (.getStylesheets root))]
      (.forget (com.sun.javafx.css.StyleManager/getInstance) scene)
      (.setAll (.getStylesheets root) ^Collection styles))))

(defn visible! [^Node node v]
  (.setVisible node v))

(defn visible? [^Node node]
  (.isVisible node))

(defn managed! [^Node node m]
  (.setManaged node m))

(defn enable! [^Node node e]
  (.setDisable node (not e)))

(defn enabled? [^Node node]
  (not (.isDisabled node)))

(defn disable! [^Node node d]
  (.setDisable node d))

(defn disabled? [^Node node]
  (.isDisabled node))

(defn window [^Scene scene]
  (.getWindow scene))

(defn close! [^Stage window]
  (.close window))

(defn title! [^Stage window t]
  (.setTitle window t))

(defn make-title
  ([] (if-some [version (system/defold-version)]
        (str "Defold " version)
        "Defold"))
  ([project-title] (str project-title " - " (make-title))))

(defn tooltip! [^Control ctrl tip localization]
  (.setTooltip ctrl (when tip (localization/localize! (Tooltip.) localization tip))))

(defn request-focus! [^Node node]
  (.requestFocus node))

(defn focus? [^Node node]
  (.isFocused node))

(defn double-click-event? [^MouseEvent event]
  (and (= MouseEvent/MOUSE_CLICKED (.getEventType event))
       (= MouseButton/PRIMARY (.getButton event))
       (= 2 (.getClickCount event))

       ;; Special handling for specific view types.
       (boolean
         (condp instance? (.getSource event)
           TreeView
           ;; Only count double-clicks on selected tree items. Ignore disclosure arrow clicks.
           (when-some [clicked-node (some-> event .getPickResult .getIntersectedNode)]
             (when-some [^javafx.scene.control.TreeCell tree-cell (closest-node-of-type javafx.scene.control.TreeCell clicked-node)]
               (when (and (.isSelected tree-cell)
                          (not (.isEmpty tree-cell)))
                 (if-some [disclosure-node (.getDisclosureNode tree-cell)]
                   (not (nodes-along-path? clicked-node disclosure-node tree-cell))
                   true))))

           ListView
           ;; Only count double-clicks on selected list items.
           (when-some [clicked-node (some-> event .getPickResult .getIntersectedNode)]
             (when-some [^ListCell tree-cell (closest-node-of-type ListCell clicked-node)]
               (and (.isSelected tree-cell)
                    (not (.isEmpty tree-cell)))))

           true))))

(defn on-double! [^Node node fn]
  (.setOnMouseClicked node (event-handler e (when (double-click-event? e)
                                              (fn e)))))

(defn on-click! [^Node node fn]
  (.setOnMouseClicked node (event-handler e (fn e))))

(defn on-mouse! [^Node node fn]
  (.setOnMouseEntered node (when fn (event-handler e (fn :enter e))))
  (.setOnMouseExited node (when fn (event-handler e (fn :exit e))))
  (.setOnMouseMoved node (when fn (event-handler e (fn :move e)))))

(defn on-key! [^Node node key-fn]
  (.setOnKeyPressed node (event-handler e (key-fn (.getCode ^KeyEvent e)))))

(defn on-focus! [^Node node focus-fn]
  (observe (.focusedProperty node)
           (fn [observable old-val got-focus]
             (focus-fn got-focus))))

(defn prevent-auto-focus! [^Node node]
  (.setFocusTraversable node false)
  (run-later (.setFocusTraversable node true)))

(defn owning-tab
  "Find the Tab that the node belongs to, if any.
  Returns nil if the node does not belong to a Tab."
  ^Tab [node]
  (when-some [tab-content-area (closest-node-with-style "tab-content-area" node)]
    (when-some [tab-pane (.getParent tab-content-area)]
      (when (instance? TabPane tab-pane)
        (some (fn [^Tab tab]
                (when-some [tab-content-parent (some-> tab .getContent .getParent)]
                  (when (identical? tab-content-area tab-content-parent)
                    tab)))
              (.getTabs ^TabPane tab-pane))))))

(declare selected-tab)

(defn focus-owner
  "Returns the Node that owns focus in the specified Scene, or nil if no node
  has input focus. This function works around a bug in JavaFX related to nodes
  inside TabPanes, and should be used in place of the .getFocusOwner method.
  The bug causes nodes that are under a deselected Tab to remain the focus owner
  at the time when the selected tab property change event fires. To avoid this
  we check if the focus owner is below a deselected tab and if so return nil."
  ^Node [^Scene scene]
  (when-some [focus-owner (.getFocusOwner scene)]
    (if-some [owning-tab (owning-tab focus-owner)]
      (when-some [selected-tab-in-owning-tab-pane (some-> owning-tab .getTabPane selected-tab)]
        (when (identical? owning-tab selected-tab-in-owning-tab-pane)
          focus-owner))
      focus-owner)))

(defn- ensure-some-focus-owner! [^Window window]
  (run-later
    (when-let [scene (.getScene window)]
      (when (nil? (focus-owner scene))
        (when-let [root (.getRoot scene)]
          (.requestFocus root))))))

(defn ensure-receives-key-events!
  "Ensures the window receives key events even if it has no FocusTraversable controls."
  [^Window window]
  (if (.isShowing window)
    (ensure-some-focus-owner! window)
    (observe-once (.showingProperty window)
                  (fn [_observable _old-val showing?]
                    (when showing?
                      (ensure-some-focus-owner! window))))))

(defn auto-commit! [^Node node commit-fn]
  (on-focus! node (fn [got-focus] (if got-focus
                                    (user-data! node ::auto-commit false)
                                    (when (user-data node ::auto-commit)
                                      (commit-fn nil)))))
  (on-edit! node (fn [_old _new]
                   (user-data! node ::auto-commit true))))

(defn clear-auto-commit! [^Node node]
  ;; Clear the auto-commit flag. You should call this whenever data has been
  ;; synced with the graph while the field still has focus. This ensures the
  ;; unedited value will not be committed to the graph unnecessarily after the
  ;; user moves focus to another control. Further edits will re-apply the
  ;; auto-commit flag, but if they leave without editing, we won't commit.
  (when (user-data node ::auto-commit)
    (user-data! node ::auto-commit false)))

(defn increase-on-edit-event-suppress-count! [editable]
  (when-some [suppress-count (user-data editable ::on-edit-event-suppress-count)]
    (user-data! editable ::on-edit-event-suppress-count (inc (long suppress-count)))))

(defn decrease-on-edit-event-suppress-count! [editable]
  (when-some [suppress-count (user-data editable ::on-edit-event-suppress-count)]
    (let [suppress-count (long suppress-count)]
      (assert (pos? suppress-count))
      (user-data! editable ::on-edit-event-suppress-count (dec suppress-count)))))

(defmacro with-on-edit-event-suppressed! [editable & body]
  `(let [editable# ~editable]
     (increase-on-edit-event-suppress-count! editable#)
     (try
       ~@body
       (finally
         (decrease-on-edit-event-suppress-count! editable#)))))

(defn- add-on-edit-event-fn! [editable observed-property listen-fn]
  (when (nil? (user-data editable ::on-edit-event-suppress-count))
    (user-data! editable ::on-edit-event-suppress-count 0))
  (observe observed-property
           (fn [_this old new]
             (let [^long suppress-count (or (user-data editable ::on-edit-event-suppress-count) 0)]
               (when (zero? suppress-count)
                 (listen-fn old new))))))

(defn- apply-default-css! [^Parent root]
  (.. root getStylesheets (add (str (io/resource "editor.css"))))
  nil)

(defn- apply-user-css! [^Parent root]
  (let [css (io/file (System/getProperty "user.home") ".defold" "editor.css")]
    (when (.exists css)
      (.. root getStylesheets (add (str (.toURI css))))))
  nil)

(defn apply-css! [^Parent root]
  (apply-default-css! root)
  (apply-user-css! root))

(defn load-fxml
  ^Parent [path]
  (let [root ^Parent (FXMLLoader/load (io/resource path))]
    (apply-user-css! root)
    root))

(defn- empty-svg []
    {:tag :svg, :attrs nil, :content [{ :tag :path, :attrs {:d "M0,0"}, :content nil}]})

(defn- load-svg-xml [path]
  (try
    (with-open [stream (io/input-stream (io/resource path))]
      (xml/parse stream))
    (catch Exception e
      (empty-svg))))

(defn load-svg-path
  "Loads the path data from a simple .svg file. Assumes the scene contains a
  single path, but is useful for things like monochrome vector icons."
  ^SVGPath [path]
  (let [svg (load-svg-xml path)
        content (:content svg)
        path (util/first-where #(= :path (:tag %)) content)
        path-data (get-in path [:attrs :d])]
    (doto (SVGPath.)
      (.setContent path-data))))

(extend-type Window
  HasUserData
  (user-data [this key] (get (.getUserData this) key))
  (user-data! [this key val] (.setUserData this (assoc (or (.getUserData this) {}) key val))))

(extend-type Scene
  HasUserData
  (user-data [this key] (get (.getUserData this) key))
  (user-data! [this key val] (.setUserData this (assoc (or (.getUserData this) {}) key val))))

(extend-type Node
  HasUserData
  (user-data [this key] (get (.getUserData this) key))
  (user-data! [this key val] (.setUserData this (assoc (or (.getUserData this) {}) key val)))
  Editable
  (editable [this] (.isDisabled this))
  (editable! [this val] (.setDisable this (not val))))

(extend-type MenuItem
  HasAction
  (on-action! [this fn] (.setOnAction this (event-handler e (fn e))))
  HasUserData
  (user-data [this key] (get (.getUserData this) key))
  (user-data! [this key val] (.setUserData this (assoc (or (.getUserData this) {}) key val))))

(extend-type Tab
  HasUserData
  (user-data [this key] (get (.getUserData this) key))
  (user-data! [this key val] (.setUserData this (assoc (or (.getUserData this) {}) key val)))
  Text
  (text [this] (.getText this))
  (text! [this val]
    (when (not= (.getText this) val)
      (.setText this val))))

(defonce/protocol HasChildren
  (children! [this c])
  (add-child! [this c]))

(defonce/protocol CollectionView
  (selection [this])
  (select! [this item])
  (select-index! [this index])
  (selection-mode! [this mode])
  (items [this])
  (items! [this items])
  (cell-factory! [this render-fn]))

(extend-type LongField
  HasValue
  (value [this] (Integer/parseInt (.getText this)))
  (value! [this val] (text! this (str val))))

(extend-type TextInputControl
  HasValue
  (value [this] (text this))
  (value! [this val] (text! this val))
  Text
  (text [this] (.getText this))
  (text! [this val]
    (with-on-edit-event-suppressed! this
      (doto this
        (.setText val)
        (.end))
      (when (.isFocused this)
        (.selectAll this))))
  Editable
  (editable [this] (.isEditable this))
  (editable! [this val] (.setEditable this val))
  (on-edit! [this f] (add-on-edit-event-fn! this (.textProperty this) f)))

(extend-type ChoiceBox
  HasAction
  (on-action! [this fn] (.setOnAction this (event-handler e (fn e))))
  HasValue
  (value [this] (.getValue this))
  (value! [this val] (with-on-edit-event-suppressed! this (.setValue this val))))

(extend-type ComboBoxBase
  Editable
  (editable [this] (not (.isDisabled this)))
  (editable! [this val] (.setDisable this (not val)))
  (on-edit! [this f] (add-on-edit-event-fn! this (.valueProperty this) f)))

(defn allow-user-input! [^ComboBoxBase cb e]
  (.setEditable cb e))

(extend-type CheckBox
  HasValue
  (value [this] (.isSelected this))
  (value! [this val] (with-on-edit-event-suppressed! this (.setSelected this val)))
  Editable
  (editable [this] (not (.isDisabled this)))
  (editable! [this val] (.setDisable this (not val)))
  (on-edit! [this f] (add-on-edit-event-fn! this (.selectedProperty this) f)))

(extend-type ColorPicker
  HasValue
  (value [this] (.getValue this))
  (value! [this val] (with-on-edit-event-suppressed! this (.setValue this val))))

(declare bind-key!)

(extend-type TextField
  HasAction
  (on-action! [node update-fn]
    (.setOnAction node (event-handler e
                         (clear-auto-commit! node)
                         (let [length (.getLength (.getSelection node))]
                           (update-fn e)
                           (if (zero? length)
                             (.selectAll node)
                             (.deselect node))))))
  Cancellable
  (on-cancel! [node cancel-fn]
    (bind-key! node "Esc" (fn []
                            (cancel-fn node)
                            (clear-auto-commit! node)
                            (when-let [parent (.getParent node)]
                              (.requestFocus parent))))))

(extend-type TextArea
  HasAction
  (on-action! [node update-fn]
    (bind-key! node "Shortcut+Enter" (fn []
                                       (clear-auto-commit! node)
                                       (let [length (.getLength (.getSelection node))]
                                         (update-fn node)
                                         (if (zero? length)
                                           (.selectAll node)
                                           (.deselect node))))))
  Cancellable
  (on-cancel! [node cancel-fn]
    (bind-key! node "Esc" (fn []
                            (cancel-fn node)
                            (clear-auto-commit! node)
                            (when-let [parent (.getParent node)]
                              (.requestFocus parent))))))

(extend-type Labeled
  Text
  (text [this] (.getText this))
  (text! [this val]
    (when (not= (.getText this) val)
      (.setText this val))))

(extend-type Label
  Text
  (text [this] (.getText this))
  (text! [this val]
    (when (not= (.getText this) val)
      (.setText this val))))

(extend-type Pane
  HasChildren
  (children! [this c]
    (doto
      (.getChildren this)
      (.clear)
      (.addAll (node-array c))))
  (add-child! [this c]
    (-> this (.getChildren) (.add c))))

(extend-type Group
  HasChildren
  (children! [this c]
    (doto
      (.getChildren this)
      (.clear)
      (.addAll (node-array c))))
  (add-child! [this c]
    (-> this (.getChildren) (.add c))))

(defn- make-style-applier []
  (let [applied-style-classes (volatile! #{})]
    (fn [^Styleable styleable style-classes]
      (when-not (set? style-classes)
        (throw (IllegalArgumentException. "style-classes must be a set")))
      (let [current @applied-style-classes
            removed ^Collection (set/difference current style-classes)
            added  ^Collection (set/difference style-classes current)]
        (doto (.getStyleClass styleable)
          (.removeAll removed)
          (.addAll added)))
      (vreset! applied-style-classes style-classes))))

(defn- make-list-cell [render-fn]
  (let [apply-style-classes! (make-style-applier)]
    (proxy [ListCell] []
      (updateItem [object empty]
        (let [^ListCell this this
              render-data (and object (render-fn object))]
          (proxy-super updateItem object empty)
          (update-list-cell-style! this)
          (if (or (nil? object) empty)
            (do
              (apply-style-classes! this #{})
              (proxy-super setText nil)
              (proxy-super setGraphic nil))
            (do
              (apply-style-classes! this (:style render-data #{}))
              (if-some [graphic (:graphic render-data)]
                (do
                  (proxy-super setText nil)
                  (proxy-super setGraphic graphic))
                (do
                  (proxy-super setText (:text render-data))
                  (when-let [icon (:icon render-data)]
                    (proxy-super setGraphic (icons/get-image-view icon 16)))))))
          (proxy-super setTooltip (:tooltip render-data)))))))

(defn- make-list-cell-factory [render-fn]
  (reify Callback (call ^ListCell [this view] (make-list-cell render-fn))))

(defn- make-tree-cell [render-fn]
  (let [apply-style-classes! (make-style-applier)
        cell (proxy [TreeCell] []
               (updateItem [item empty]
                 (let [^TreeCell this this
                       render-data (and item (render-fn item))]
                   (proxy-super updateItem item empty)
                   (update-tree-cell-style! this)
                   (if empty
                     (do
                       (apply-style-classes! this #{})
                       (proxy-super setText nil)
                       (proxy-super setGraphic nil)
                       (proxy-super setTooltip nil)
                       (proxy-super setOnDragOver nil)
                       (proxy-super setOnDragDropped nil)
                       (proxy-super setOnDragEntered nil)
                       (proxy-super setOnDragExited nil))
                     (do
                       (apply-style-classes! this (:style render-data #{}))
                       (if-some [graphic (:graphic render-data)]
                         (do
                           (proxy-super setText nil)
                           (proxy-super setGraphic graphic))
                         (do
                           (proxy-super setText (:text render-data))
                           (proxy-super setGraphic (when-let [icon (:icon render-data)]
                                                     (if (= :empty icon)
                                                       (ImageView.)
                                                       (icons/get-image-view icon (:icon-size render-data 16)))))))
                       (proxy-super setTooltip (:tooltip render-data))
                       (proxy-super setOnDragOver (:over-handler render-data))
                       (proxy-super setOnDragDropped (:dropped-handler render-data))
                       (proxy-super setOnDragEntered (:entered-handler render-data))
                       (proxy-super setOnDragExited (:exited-handler render-data)))))))]
    cell))

(defn- make-tree-cell-factory [render-fn]
  (reify Callback (call ^TreeCell [this view] (make-tree-cell render-fn))))

(extend-type ButtonBase
  HasAction
  (on-action! [this fn] (.setOnAction this (event-handler e (fn e)))))

(extend-type ColorPicker
  HasAction
  (on-action! [this fn] (.setOnAction this (event-handler e (fn e)))))

(extend-type ComboBox
  CollectionView
  (selection [this]
    (when-let [item (.getSelectedItem (.getSelectionModel this))]
      [item]))
  (select! [this item]
    (doto (.getSelectionModel this)
      (.select item)))
  (select-index! [this index]
    (doto (.getSelectionModel this)
      (.select (int index))))
  (selection-mode! [this mode]
    (when (= :multiple mode)
      (throw (UnsupportedOperationException. "ComboBox only has single selection"))))
  (items [this]
    (.getItems this))
  (items! [this ^Collection items]
    (let [l (.getItems this)]
      (.clear l)
      (.addAll l items)))
  (cell-factory! [this render-fn]
    (.setButtonCell this (make-list-cell render-fn))
    (.setCellFactory this (make-list-cell-factory render-fn))))

(defn- selection-mode ^SelectionMode [mode]
  (case mode
    :single SelectionMode/SINGLE
    :multiple SelectionMode/MULTIPLE))

(extend-type ListView
  CollectionView
  (selection [this]
    (when-let [items (.getSelectedItems (.getSelectionModel this))]
      items))
  (select! [this item]
    (doto (.getSelectionModel this)
      (.select item)))
  (select-index! [this index]
    (doto (.getSelectionModel this)
      (.select (int index))))
  (selection-mode! [this mode]
    (let [^SelectionMode mode (selection-mode mode)]
      (.setSelectionMode (.getSelectionModel this) mode)))
  (items [this]
    (.getItems this))
  (items! [this ^Collection items]
    (let [l (.getItems this)]
      (.clear l)
      (.addAll l items)))
  (cell-factory! [this render-fn]
    (.setCellFactory this (make-list-cell-factory render-fn))))

(defn tree-item-seq [item]
  (if item
    (tree-seq
      #(not (.isLeaf ^TreeItem %))
      #(seq (.getChildren ^TreeItem %))
      item)
    []))

(defn select-indices!
  [^TreeView tree-view indices]
  (let [selection-model (.getSelectionModel tree-view)]
    (doseq [^long index indices]
      (.select selection-model index))))

(extend-type TreeView
  CollectionView
  (selection [this] (->> this
                         .getSelectionModel
                         .getSelectedItems
                         (keep #(when % (.getValue ^TreeItem %)))
                         (into [])))
  (select! [this item] (let [tree-items (tree-item-seq (.getRoot this))]
                         (when-let [tree-item (some (fn [^TreeItem tree-item] (and (= item (.getValue tree-item)) tree-item)) tree-items)]
                           (doto (.getSelectionModel this)
                             (.clearSelection)
                             ;; This will not scroll the tree-view to display the item, which is an open issue in JavaFX. The known workaround is to access the internal classes, which seems like a bad idea.
                             (.select tree-item)))))
  (select-index! [this index] (doto (.getSelectionModel this)
                                (.select (int index))))
  (selection-mode! [this mode] (let [^SelectionMode mode (selection-mode mode)]
                                 (.setSelectionMode (.getSelectionModel this) mode)))
  (cell-factory! [this render-fn]
    (.setCellFactory this (make-tree-cell-factory render-fn))))

(extend-type TableView
  CollectionView
  (selection [this]
    (when-let [items (.getSelectedItems (.getSelectionModel this))]
      items))
  (select! [this item]
    (doto (.getSelectionModel this)
      (.select item)))
  (select-index! [this index]
    (doto (.getSelectionModel this)
      (.select (int index))))
  (selection-mode! [this mode]
    (let [^SelectionMode mode (selection-mode mode)]
      (.setSelectionMode (.getSelectionModel this) mode)))
  (items [this]
    (.getItems this))
  (items! [this ^Collection items]
    (let [l (.getItems this)]
      (.clear l)
      (.addAll l items)))
  (cell-factory! [this render-fn]
    ;; NOTE: TableView uses column-specific cell factories so leave this empty for now At the time
    ;; of implementing this, it's only being used for the selection stuff in breakpoints-view
    nil))

(defn selection-root-items
  "Make a vector of TreeItems that correspond to selection roots

  Args:
    tree-view    TreeView instance
    id-fn        function of TreeItem's value that returns its id; multiple
                 TreeItems may share an id, in this case, only one of them will
                 be returned; if id is falsey, the TreeItem is excluded"
  [^TreeView tree-view id-fn]
  (let [tree-item-id-fn (comp id-fn TreeItem/.getValue)
        path->tree-item (->> tree-view
                             .getSelectionModel
                             .getSelectedItems
                             (e/filter #(and % (tree-item-id-fn %)))
                             (coll/pair-map-by
                               (fn path-fn [^TreeItem item]
                                 (vec (rseq (coll/transfer (iterate TreeItem/.getParent item) []
                                              (take-while some?)
                                              (map tree-item-id-fn)))))))
        root-paths (reduce-kv
                     (fn [root-paths path _]
                       (if (coll/any? #(util/seq-starts-with? path %) root-paths)
                         root-paths
                         (conj root-paths path)))
                     []
                     path->tree-item)]
    (coll/transfer root-paths []
      (map path->tree-item)
      (util/distinct-by tree-item-id-fn))))

;; Returns the items that should be selected if the specified root items were deleted.
(defn succeeding-selection [^TreeView tree-view root-items]
  (let [items (sort-by (fn [^TreeItem item] (.getRow tree-view item)) root-items)
        ^TreeItem last-item (last items)
        indices (set (mapv (fn [^TreeItem item] (.getRow tree-view item)) root-items))
        ^TreeItem next-item (if last-item
                              (or (.nextSibling last-item)
                                  (loop [^TreeItem item last-item]
                                    (if-let [^TreeItem prev (.previousSibling item)]
                                      (if (contains? indices (.getRow tree-view prev))
                                        (recur prev)
                                        prev)
                                      (.getParent last-item))))
                              (.getRoot tree-view))]
    [(.getValue next-item)]))

(defn scroll-to-item!
  [^TreeView tree-view ^TreeItem tree-item]
  (let [row (.getRow tree-view tree-item)]
    (when-not (= -1 row)
      (.scrollTo tree-view row))))

(defn- custom-tree-view-key-pressed! [^KeyEvent event]
  ;; The TreeView control consumes Space key presses internally and does
  ;; something weird and undesirable to the selection. Instead, we consume and
  ;; redirect the keypress to the Scene root so that we can use the Space key
  ;; for keyboard shortcuts even if a TreeView has input focus.
  (when (and (= KeyCode/SPACE (.getCode event))
             (not (or (.isAltDown event)
                      (.isControlDown event)
                      (.isMetaDown event)
                      (.isShiftDown event))))
    (let [^Node source (.getSource event)]
      (when (instance? Node source)
        (when-some [scene-root (some-> source .getScene .getRoot)]
          (let [redirected-event (.copyFor event source scene-root)]
            (.consume event)
            (.fireEvent scene-root redirected-event)))))))

(defn- custom-tree-view-mouse-pressed! [^MouseEvent event]
  (when (= MouseButton/PRIMARY (.getButton event))
    (let [target (.getTarget event)]
      ;; Did the user click on a tree cell?
      (when-some [^TreeCell tree-cell (closest-node-of-type TreeCell target)]
        (when-some [disclosure-node (.getDisclosureNode tree-cell)]
          ;; Did the user click on the disclosure node?
          (when (nodes-along-path? target disclosure-node tree-cell)
            ;; Consume the event and manually toggle the expanded state for the
            ;; associated tree item. If the Alt modifier key is held, recursively
            ;; set the expanded state for all children.
            (when-some [tree-item (.getTreeItem tree-cell)]
              (.consume event)
              (let [new-expanded-state (not (.isExpanded tree-item))]
                (if-not (.isAltDown event)
                  (.setExpanded tree-item new-expanded-state)
                  (doseq [^TreeItem tree-item (tree-item-seq tree-item)]
                    (.setExpanded tree-item new-expanded-state)))))))))))

(def ^:private custom-tree-view-key-pressed-event-filter
  (event-handler event
    (custom-tree-view-key-pressed! event)))

(def ^:private custom-tree-view-mouse-pressed-event-filter
  (event-handler event
    (custom-tree-view-mouse-pressed! event)))

(def ignore-event-filter
  (event-handler event
    (.consume ^Event event)))

(defn customize-tree-view!
  "Customize the behavior of the supplied tree-view for our purposes. Currently
  the following customizations are applied:
  * The Space key no longer does something weird to the selection. Instead we
    redirect it to the Scene root so it can be used for keyboard shortcuts.
  * Alt-clicking on an items disclosure arrow toggles expansion state for all
    child items recursively.

  Additional opts:
  * :double-click-expand?
    If true, double-clicking will toggle expansion of a tree item."
  [^TreeView tree-view opts]
  (.addEventFilter tree-view KeyEvent/KEY_PRESSED custom-tree-view-key-pressed-event-filter)
  (.addEventFilter tree-view MouseEvent/MOUSE_PRESSED custom-tree-view-mouse-pressed-event-filter)
  (when-not (:double-click-expand? opts)
    (.addEventFilter tree-view MouseEvent/MOUSE_RELEASED ignore-event-filter)))

(extend-protocol HasSelectionModel
  ChoiceBox     (selection-model [this] (.getSelectionModel this))
  ComboBox      (selection-model [this] (.getSelectionModel this))
  ListView      (selection-model [this] (.getSelectionModel this))
  TableView     (selection-model [this] (.getSelectionModel this))
  TabPane       (selection-model [this] (.getSelectionModel this))
  TreeTableView (selection-model [this] (.getSelectionModel this))
  TreeView      (selection-model [this] (.getSelectionModel this)))

(defn observe-selection
  "Helper function that lets you observe selection changes in a generic fashion.
  Takes a Node that satisfies HasSelectionModel and a function that takes the
  reporting Node and a vector with the selected items as its arguments.

  Supports both single and multi-selection. In both cases the selected items
  will be provided in a vector."
  [node listen-fn]
  (let [selection-model (selection-model node)]
    (if (instance? MultipleSelectionModel selection-model)
      (observe-list node
                    (.getSelectedItems ^MultipleSelectionModel selection-model)
                    (fn [_ _]
                      (listen-fn node (selection node))))
      (observe (.selectedItemProperty selection-model)
               (fn [_ _ _]
                 (listen-fn node (selection node)))))))

;; context handling

;; context for TextInputControls so that we can retain default behaviours

(defn make-text-input-control-context
  [control]
  (handler/->context :text-input-control {:control control}))

(defn- has-selection?
  [^TextInputControl control]
  (not (string/blank? (.getSelectedText control))))

(handler/defhandler :edit.cut :text-input-control
  (enabled? [^TextInputControl control] (and (editable control) (has-selection? control)))
  (run [^TextInputControl control] (.cut control)))

(handler/defhandler :edit.copy :text-input-control
  (enabled? [^TextInputControl control] (has-selection? control))
  (run [^TextInputControl control] (.copy control)))

(handler/defhandler :edit.paste :text-input-control
  (enabled? [^TextInputControl control] (and (editable control) (.. Clipboard getSystemClipboard hasString)))
  (run [^TextInputControl control] (.paste control)))

(handler/defhandler :edit.delete :text-input-control
  (enabled? [^TextInputControl control] (editable control))
  (run [^TextInputControl control] (.deleteNextChar control)))

(handler/defhandler :edit.undo :text-input-control
  (enabled? [^TextInputControl control] (.isUndoable control))
  (run [^TextInputControl control] (.undo control)))

(handler/defhandler :edit.redo :text-input-control
  (enabled? [^TextInputControl control] (.isRedoable control))
  (run [^TextInputControl control] (.redo control)))

(defn ->selection-provider [view]
  (reify handler/SelectionProvider
    (selection [_this _evaluation-context] (selection view))
    (succeeding-selection [_this _evaluation-context] [])
    (alt-selection [_this _evaluation-context] [])))

(defn context!
  ([^Node node name env selection-provider]
   (context! node name env selection-provider {}))
  ([^Node node name env selection-provider dynamics]
   (context! node name env selection-provider dynamics {}))
  ([^Node node name env selection-provider dynamics adapters]
   (user-data! node ::context (handler/->context name env selection-provider dynamics adapters))))

(defn context
  [^Node node]
  (cond
    (instance? TabPane node)
    (let [^TabPane tab-pane node
          ^Tab tab (.getSelectedItem (.getSelectionModel tab-pane))]
      (or (and tab (.getContent tab) (user-data (.getContent tab) ::context))
          (user-data node ::context)))

    (instance? TextInputControl node)
    (make-text-input-control-context node)

    :else
    (user-data node ::context)))

(defn node-contexts
  [^Node initial-node all-selections? evaluation-context]
  (loop [^Node node initial-node
         ctxs []]
    (if-not node
      (handler/eval-contexts ctxs all-selections? evaluation-context)
      (if-let [ctx (context node)]
        (recur (.getParent node) (conj ctxs ctx))
        (recur (.getParent node) ctxs)))))

(defn contexts
  ([^Scene scene all-selections?]
   (g/with-auto-evaluation-context evaluation-context
     (contexts scene all-selections? evaluation-context)))
  ([^Scene scene all-selections? evaluation-context]
   (node-contexts (or (focus-owner scene) (.getRoot scene)) all-selections? evaluation-context)))

(defn resolve-handler-ctx [command-contexts command user-data]
  (let [handler-ctx (handler/active command command-contexts user-data)]
    (cond
      (nil? handler-ctx)
      ::not-active

      (not (handler/enabled? handler-ctx)) ; Safe to not supply evaluation-context - we're executing a command.
      ::not-enabled

      :else
      handler-ctx)))

(defn execute-handler-ctx [handler+command-context]
  (let [ret (handler/run handler+command-context)]
    (user-data! (main-scene) ::refresh-requested? true)
    ret))

(defn execute-command
  [command-contexts command user-data]
  (let [handler-ctx (resolve-handler-ctx command-contexts command user-data)]
    (case handler-ctx
      (::not-active ::not-enabled) handler-ctx
      (execute-handler-ctx handler-ctx))))

(defn- select-items [items options command-contexts]
  (execute-command command-contexts :private/select-items {:items items :options options}))

(defn image-icon
  "Cljfx image view with image loaded from classpath or workspace

  Required props:

    :path    image path

  Optional props (in addition to image-view props):

    :size    image size, a double"
  [{:keys [path size] :as props}]
  (let [desc (-> props
                 (assoc :fx/type fx.image-view/lifecycle)
                 (dissoc :path :size))]
    (if size
      (assoc desc :image (icons/get-image path) :fit-width size :fit-height size)
      (assoc desc :image (icons/get-image path)))))

(defn invoke-handler
  ([command-contexts command]
   (invoke-handler command-contexts command nil))
  ([command-contexts command user-data]
   (if-let [handler-ctx (handler/active command command-contexts user-data)]
     (if-let [options (and (nil? user-data) (handler/option-items handler-ctx))]
       (when-let [user-data (some-> (select-items
                                      options
                                      {:title (handler/label handler-ctx)
                                       :filter-on (fn [{:keys [label]} localization]
                                                    (localization label))
                                       :cell-fn (fn [{:keys [label icon]} localization]
                                                  (cond-> {:text (localization label)}
                                                          icon
                                                          (assoc :graphic {:fx/type image-icon
                                                                           :path icon
                                                                           :size 16.0})))}
                                      command-contexts)
                                    first
                                    :user-data)]
         (execute-command command-contexts command user-data))
       (execute-command command-contexts command user-data))
     ::not-active)))

(defn execute-accelerator-commands [commands]
  ;; It is imperative that the handler is invoked using run-later as
  ;; this avoids a JVM crash on some macOS versions. Prior to macOS
  ;; Sierra, the order in which native menu events are delivered
  ;; triggered a segfault in the native menu implementation when the
  ;; stage is changed during the event dispatch. This happens for
  ;; example when we have a shortcut triggering the opening of a
  ;; dialog.
  (run-later (let [command-contexts (contexts (main-scene) true)]
               (reduce
                 (fn [acc command]
                   (let [ret (invoke-handler command-contexts command)]
                     (case ret
                       (::not-active ::not-enabled) acc
                       (reduced ret))))
                 nil
                 commands))))

(defn- make-desc [control menu-id]
  {:control control
   :menu-id menu-id})

(defn- make-submenu [id label localization icon ^Collection style-classes menu-items on-open]
  (when (seq menu-items)
    (let [menu (Menu.)]
      (localization/localize! menu localization label)
      (user-data! menu ::menu-item-id id)
      (when on-open
        (.setOnShowing menu (event-handler e (on-open))))
      (when icon
        (.setGraphic menu (icons/get-image-view icon 16)))
      (when style-classes
        (assert (set? style-classes))
        (doto (.getStyleClass menu)
          (.addAll style-classes)))
      (.addAll (.getItems menu) (to-array menu-items))
      menu)))

(defonce/type MenuEventHandler [^Scene scene command user-data ^:unsynchronized-mutable suppress?]
  EventHandler
  (handle [_this event]
    (condp = (.getEventType event)
      MenuItem/MENU_VALIDATION_EVENT
      (set! suppress? true)

      ActionEvent/ACTION
      (try
        (when-not suppress? (invoke-handler (contexts scene true) command user-data))
        (finally
          (set! suppress? false))))))

(defn- make-menu-command [^Scene scene id label localization icon ^Collection style-classes key-combo user-data command enabled? check]
  (let [^MenuItem menu-item (if check (CheckMenuItem.) (MenuItem.))]
    (localization/localize! menu-item localization label)
    (user-data! menu-item ::menu-item-id id)
    (when command
      (user-data! menu-item ::command command))
    (when (and (some? key-combo)
               ;; Currently not allowed due to a problem on macOS. See below.
               ;; Still a problem in JavaFX 23.
               (not check)
               (nil? user-data))
      (.setAccelerator menu-item key-combo))
    (when icon
      (.setGraphic menu-item (icons/get-image-view icon 16)))
    (when style-classes
      (assert (set? style-classes))
      (doto (.getStyleClass menu-item)
        (.addAll style-classes)))
    (.setDisable menu-item (not enabled?))
    (if (os/is-mac-os?)
      ;; On macOS, there is no way to prevent a shortcut handled by a
      ;; scene accelerator from also triggering a MenuItem with the
      ;; same accelerator. To avoid invoking the command twice, we use
      ;; a special event handler that suppresses actions invoked via
      ;; an accelerator, but still dispatches actions when invoked by
      ;; clicking the menu item.
      ;; Note this doesn't seem to work for CheckMenuItems as the
      ;; CheckMenuItemAdapter in GlobalMenuAdapter.java does
      ;; getOnMenuValidation() on this instead of the target
      ;; menuItem. In effect, we never get a MENU_VALIDATION_EVENT.
      ;; See https://github.com/openjdk/jfx/blob/master/modules/javafx.controls/src/main/java/com/sun/javafx/scene/control/GlobalMenuAdapter.java
      (let [handler (->MenuEventHandler scene command user-data false)]
        (.setOnMenuValidation menu-item handler)
        (.setOnAction menu-item handler))
      (.setOnAction menu-item (event-handler event (invoke-handler (contexts scene true) command user-data))))
    (user-data! menu-item ::menu-user-data user-data)
    menu-item))

(declare make-menu-items)

(defn- make-menu-item [^Scene scene item command-contexts keymap localization evaluation-context]
  (let [id (:id item)
        icon (:icon item)
        style-classes (:style item)
        item-label (:label item)
        on-open (:on-submenu-open item)]
    (if-let [children (:children item)]
      (make-submenu id
                    item-label
                    localization
                    icon
                    style-classes
                    (make-menu-items scene children command-contexts keymap localization evaluation-context)
                    on-open)
      (if (= item-label :separator)
        (SeparatorMenuItem.)
        (let [command (:command item)
              user-data (:user-data item)
              check (:check item)]
          (when-let [handler-ctx (handler/active command command-contexts user-data evaluation-context)]
            (let [label (or (handler/label handler-ctx evaluation-context) item-label) ; Note that this is *not* updated on every menu refresh. Can't do "Show X" <-> "Hide X".
                  enabled? (handler/enabled? handler-ctx evaluation-context)
                  key-combo (first (keymap/shortcuts keymap command))]
              (if-let [options (handler/options handler-ctx evaluation-context)]
                (if (and key-combo (not (:expand item)))
                  (make-menu-command scene id label localization icon style-classes key-combo user-data command enabled? check)
                  (make-submenu id
                                label
                                localization
                                icon
                                style-classes
                                (make-menu-items scene (localization/sort-if-annotated @localization options) command-contexts keymap localization evaluation-context)
                                on-open))
                (make-menu-command scene id label localization icon style-classes key-combo user-data command enabled? check)))))))))

(defn- make-menu-items [^Scene scene menu command-contexts keymap localization evaluation-context]
  (into []
        (keep #(make-menu-item scene % command-contexts keymap localization evaluation-context))
        menu))

(defn- make-context-menu ^ContextMenu [menu-items]
  (let [context-menu (doto (ContextMenu.)
                       (.setConsumeAutoHidingEvents true))]
    (.addAll (.getItems context-menu) (to-array menu-items))
    context-menu))

(declare refresh-separator-visibility)
(declare refresh-menu-item-styles)

(let [method (.getDeclaredMethod ContextMenu
                                 "setShowRelativeToWindow"
                                 (into-array Class [Boolean/TYPE]))]
  (.setAccessible method true)
  (defn set-show-relative-to-window! [context-menu x]
    (.invoke method context-menu (into-array Object [(boolean x)]))))

(defn init-context-menu! ^ContextMenu [menu-location ^Scene scene]
  (let [menu-items (g/with-auto-or-fake-evaluation-context evaluation-context
                     (make-menu-items scene (handler/realize-menu menu-location) (contexts scene false evaluation-context) (or (user-data scene :keymap) keymap/empty) (user-data scene :localization) evaluation-context))
        cm (make-context-menu menu-items)]
    (doto (.getItems cm)
      (refresh-separator-visibility)
      (refresh-menu-item-styles))
    ;; Required for autohide to work when the event originates from the anchor/source node
    ;; See RT-15160 and Control.java
    (set-show-relative-to-window! cm true)
    cm))

(defn- show-context-menu! [menu-location ^ContextMenuEvent event]
  (when-not (.isConsumed event)
    (.consume event)
    (let [node ^Node (.getTarget event)
          scene ^Scene (.getScene node)
          context-menu (init-context-menu! menu-location scene)]
      (.show context-menu node (.getScreenX event) (.getScreenY event)))))

(defn register-context-menu
  "Register a context menu listener on a control for the menu location

  Args:
    control          an instance of a Control
    menu-location    keyword identifier of a menu location registered with
                     [[editor.handler/register-menu!]]
    focus            whether to focus on the control when the context menu is
                     requested, default false. Focusing might be necessary in
                     some cases because the context menu handlers are evaluated
                     in the context of the focus owner of the scene, and some
                     controls (e.g. Label) are not focused when a context menu
                     is requested for them. Without a focus, any context these
                     controls define will be lost."
  ([control menu-location]
   (register-context-menu control menu-location false))
  ([^Control control menu-location focus]
   (.addEventHandler control ContextMenuEvent/CONTEXT_MENU_REQUESTED
     (event-handler event
       (when focus (.requestFocus control))
       (show-context-menu! menu-location event)))))

(defn register-button-menu
  [^MenuButton menu-button menu-location]
  (.setOnShowing
    menu-button
    (event-handler event
      (.consume event)
      (let [scene (scene menu-button)
            menu (handler/realize-menu menu-location)
            command-contexts (contexts scene false)
            keymap (or (user-data scene :keymap) keymap/empty)
            localization (user-data scene :localization)
            _ (assert (some? localization))

            menu-items
            (g/with-auto-or-fake-evaluation-context evaluation-context
              (make-menu-items scene menu command-contexts keymap localization evaluation-context))]

        (refresh-separator-visibility menu-items)
        (refresh-menu-item-styles menu-items)
        (.setAll (.getItems menu-button) (to-array menu-items))
        nil))))

(defn- event-targets-tab? [^Event event]
  (some? (closest-node-with-style "tab" (.getTarget event))))

(defn register-tab-pane-context-menu [^TabPane tab-pane menu-location]
  (.addEventHandler tab-pane ContextMenuEvent/CONTEXT_MENU_REQUESTED
    (event-handler event
      (when (event-targets-tab? event)
        (show-context-menu! menu-location event)))))

(defn disable-menu-alt-key-mnemonic!
  "On Windows, the bare Alt KEY_PRESSED event causes the input focus to move to the menu bar.
  This function disables this behavior by replacing the Runnable that normally
  selects the first menu item with a noop."
  [^MenuBar menu-bar]
  (let [menu-bar-skin (.getSkin menu-bar)
        first-menu-runnable-field (doto (.. menu-bar-skin getClass (getDeclaredField "firstMenuRunnable"))
                                    (.setAccessible true))]
    (.set first-menu-runnable-field menu-bar-skin (fn []))))

(defn register-menubar [^Scene scene menubar menu-id]
  ;; TODO: See comment below about top-level items. Should be enforced here
 (let [root (.getRoot scene)]
   (let [desc (make-desc menubar menu-id)]
     (user-data! root ::menubar desc))))

(defn run-command
  ([^Node node command]
   (run-command node command {}))
  ([^Node node command user-data]
   (run-command node command user-data true nil))
  ([^Node node command user-data all-selections? success-fn]
   (g/let-ec [user-data (or user-data {})
              command-contexts (node-contexts node all-selections? evaluation-context)]
     (let [ret (execute-command command-contexts command user-data)]
       (when (and (not= ::not-active ret)
                  (not= ::not-enabled ret))
         (when (some? success-fn)
           (success-fn))
         ret)))))

(defn bind-action!
  ([^Node node command]
   (bind-action! node command {}))
  ([^Node node command user-data]
   (user-data! node ::bound-action {:command command :user-data user-data})
   (on-action! node (fn [^Event e] (run-command node command user-data true (fn [] (.consume e)))))))

(defn bound-action-enabled?
  [^Node node evaluation-context]
  (let [{:keys [command user-data]
         :or {user-data {}}} (user-data node ::bound-action)
        command-contexts (node-contexts node true evaluation-context)
        handler-ctx (handler/active command command-contexts user-data evaluation-context)]
    (and handler-ctx
         (handler/enabled? handler-ctx evaluation-context))))

(defn bind-double-click!
  ([^Node node command]
   (bind-double-click! node command {}))
  ([^Node node command user-data]
   (.addEventFilter node MouseEvent/MOUSE_CLICKED
                    (event-handler e
                      (when (double-click-event? e)
                        (run-command node command user-data false (fn [] (.consume e))))))))

(defn bind-key! [^Node node acc f]
  (let [combo (KeyCombination/keyCombination acc)]
    (.addEventFilter node KeyEvent/KEY_PRESSED
                     (event-handler event
                                    (when (.match combo event)
                                      (f)
                                      (.consume event))))))

(defn bind-keys! [^Node node key-bindings]
  (.addEventFilter node KeyEvent/KEY_PRESSED
    (event-handler event
                   (let [code (.getCode ^KeyEvent event)]
                     (when-let [binding (get key-bindings code)]
                       (let [[command user-data] (if (vector? binding)
                                                   binding
                                                   [binding {}])]
                         (run-command node command user-data true (fn [] (.consume event)))))))))

(defn bind-key-commands!
  "A more flexible version of bind-keys! that supports modifier keys. Accelerators are specified as strings, which are
  resolved into KeyCombinations. Bindings can be either command keywords or a two-element vector of command-keyword
  and user-data map."
  [^Node node bindings-by-acc]
  (let [bindings-by-combo (into []
                                (map (fn [[^String acc binding]]
                                       (let [combo (KeyCombination/keyCombination acc)]
                                         (assert (= acc (.getName combo))
                                                 (str "Invalid key combination: " (pr-str acc)))
                                         (assert (or (keyword? binding)
                                                     (and (vector? binding)
                                                          (= 2 (count binding))
                                                          (keyword? (first binding))
                                                          (map? (second binding))))
                                                 (str "Invalid binding: " (pr-str binding)))
                                         [combo binding])))
                                bindings-by-acc)]
    (.addEventFilter node KeyEvent/KEY_PRESSED
                     (event-handler event
                                    (when-some [binding (some (fn [[^KeyCombination combo binding]]
                                                                (when (.match combo event)
                                                                  binding))
                                                              bindings-by-combo)]
                                      (let [[command user-data] (if (vector? binding)
                                                                  binding
                                                                  [binding {}])]
                                        (run-command node command user-data true #(.consume event))))))))

;;--------------------------------------------------------------------
;; menus

(defonce ^:private invalid-menubar-items (atom #{}))

(defn invalidate-menubar-item!
  [id]
  (swap! invalid-menubar-items conj id)
  (user-data! (main-scene) ::refresh-requested? true))

(defn- clear-invalidated-menubar-items!
  []
  (reset! invalid-menubar-items #{}))

(defonce/protocol HasMenuItemList
  (menu-items ^ObservableList [this] "returns a ObservableList of MenuItems or nil"))

(extend-protocol HasMenuItemList
  MenuBar
  (menu-items [this] (.getMenus this))

  ContextMenu
  (menu-items [this] (.getItems this))

  Menu
  (menu-items [this] (.getItems this))

  MenuItem
  (menu-items [this])

  CheckMenuItem
  (menu-items [this]))

(defn- replace-menu!
  [^MenuBar menu-bar ^MenuItem old ^MenuItem new]
  (when-some [parent (or (.getParentMenu old) menu-bar)]
    (when-some [parent-children (menu-items parent)]
      (let [index (.indexOf parent-children old)]
        (when-not (neg? index)
          (.set parent-children index new))))))

(defn- refresh-menubar? [menu-bar menu visible-command-contexts keymap]
  (or (not= menu (user-data menu-bar ::menu))
      (not= visible-command-contexts (user-data menu-bar ::visible-command-contexts))
      (not= keymap (user-data menu-bar ::keymap))))

(defn- refresh-menubar! [^MenuBar menu-bar menu visible-command-contexts keymap localization evaluation-context]
  (.clear (.getMenus menu-bar))
  ;; TODO: We must ensure that top-level element are of type Menu and note MenuItem here, i.e. top-level items with ":children"
  (.addAll (.getMenus menu-bar)
           ^Collection (make-menu-items (.getScene menu-bar)
                                        menu
                                        visible-command-contexts
                                        keymap
                                        localization
                                        evaluation-context))
  (user-data! menu-bar ::menu menu)
  (user-data! menu-bar ::visible-command-contexts visible-command-contexts)
  (user-data! menu-bar ::keymap keymap)
  (clear-invalidated-menubar-items!))

(defn- refresh-menubar-items?
  []
  (seq @invalid-menubar-items))

(defn- menu->id-map
  [menu]
  (into {}
        (comp
          (filter #(instance? MenuItem %))
          (keep (fn [^MenuItem m]
                  (when-some [id (user-data m ::menu-item-id)]
                    [(keyword id) m]))))
        (tree-seq #(seq (menu-items %)) #(menu-items %) menu)))

(defn- menu-data->id-map
  [menu-data]
  (into {}
        (keep (fn [menu-data-entry]
                (when-some [id (:id menu-data-entry)]
                  [id menu-data-entry])))
        (tree-seq :children :children {:children menu-data})))

(defn- menu-data-without-icons [menu-data]
  (util/deep-keep-kv
    (fn [key value]
      (when (not= :icon key)
        value))
    menu-data))

(defn- refresh-menubar-items!
  [^MenuBar menu-bar menu-data visible-command-contexts keymap localization evaluation-context]
  (let [id->menu-item (menu->id-map menu-bar)
        id->menu-data (menu-data->id-map menu-data)]
    (doseq [id @invalid-menubar-items]
      (let [^MenuItem menu-item (id->menu-item id)
            menu-item-data (id->menu-data id)]
        (when (and menu-item menu-item-data)
          (let [new-menu-item (make-menu-item (.getScene menu-bar)
                                              menu-item-data
                                              visible-command-contexts
                                              keymap
                                              localization
                                              evaluation-context)]
            (replace-menu! menu-bar menu-item new-menu-item)))))
    (clear-invalidated-menubar-items!)))

(defn- refresh-separator-visibility [menu-items]
  (loop [menu-items menu-items
         ^MenuItem last-visible-non-separator-item nil
         ^MenuItem pending-separator nil]
    (when-let [^MenuItem child (first menu-items)]
      (if (.isVisible child)
        (condp instance? child
          Menu
          (do
            (refresh-separator-visibility (.getItems ^Menu child))
            (some-> pending-separator (.setVisible true))
            (recur (rest menu-items)
                   child
                   nil))

          SeparatorMenuItem
          (do
            (.setVisible child false)
            (recur (rest menu-items)
                   last-visible-non-separator-item
                   (when last-visible-non-separator-item child)))

          MenuItem
          (do
            (some-> pending-separator (.setVisible true))
            (recur (rest menu-items)
                   child
                   nil)))
        (recur (rest menu-items)
               last-visible-non-separator-item
               pending-separator)))))

(defn- refresh-menu-item-state [^MenuItem menu-item command-contexts evaluation-context]
  (condp instance? menu-item
    Menu
    (let [^Menu menu menu-item]
      (let [child-menu-items (seq (.getItems menu))]
        (doseq [child-menu-item child-menu-items]
          (refresh-menu-item-state child-menu-item command-contexts evaluation-context))
        (let [visible (boolean (some #(and (not (instance? SeparatorMenuItem %)) (.isVisible ^MenuItem %)) child-menu-items))]
          (.setVisible menu visible))))

    CheckMenuItem
    (let [^CheckMenuItem check-menu-item menu-item
          command (user-data check-menu-item ::command)
          user-data (user-data check-menu-item ::menu-user-data)
          handler-ctx (handler/active command command-contexts user-data evaluation-context)]
      (doto check-menu-item
        (.setDisable (not (handler/enabled? handler-ctx evaluation-context)))
        (.setSelected (boolean (handler/state handler-ctx evaluation-context)))))

    MenuItem
    (let [handler-ctx (handler/active (user-data menu-item ::command)
                                      command-contexts
                                      (user-data menu-item ::menu-user-data)
                                      evaluation-context)
          disable (not (and handler-ctx (handler/enabled? handler-ctx evaluation-context)))]
      (.setDisable menu-item disable))))

(defn- refresh-menu-item-styles [menu-items]
  (let [visible-menu-items (filter #(.isVisible ^MenuItem %) menu-items)]
    (some-> (first visible-menu-items) (add-style! "first-menu-item"))
    (doseq [middle-item (rest (butlast visible-menu-items))]
      (remove-styles! middle-item ["first-menu-item" "last-menu-item"]))
    (some-> (last visible-menu-items) (add-style! "last-menu-item"))
    (doseq [^Menu menu (filter #(instance? Menu %) visible-menu-items)]
      (refresh-menu-item-styles (.getItems menu)))))

(defn- refresh-menubar-state [^MenuBar menubar current-command-contexts evaluation-context]
  (doseq [^Menu m (.getMenus menubar)]
    (refresh-menu-item-state m current-command-contexts evaluation-context)
    ;; The system menu bar on osx seems to handle consecutive
    ;; separators and using .setVisible to hide a SeparatorMenuItem
    ;; makes the entire containing submenu appear empty.
    (when-not (and (os/is-mac-os?)
                   (.isUseSystemMenuBar menubar))
      (refresh-separator-visibility (.getItems m)))
    (refresh-menu-item-styles (.getItems m)))
  (user-data! menubar ::current-command-contexts current-command-contexts))

(defn register-toolbar [^Scene scene ^Node context-node toolbar-id menu-id]
  (let [root (.getRoot scene)]
    (if-let [toolbar (.lookup context-node toolbar-id)]
      (let [desc (make-desc toolbar menu-id)]
        (user-data! root ::toolbars (assoc (user-data root ::toolbars) [context-node toolbar-id] desc)))
      (log/warn :message (format "toolbar %s not found" toolbar-id)))))

(defn unregister-toolbar [^Scene scene ^Node context-node toolbar-id]
  (let [root (.getRoot scene)]
    (if (some? (.lookup context-node toolbar-id))
      (user-data! root ::toolbars (dissoc (user-data root ::toolbars) [context-node toolbar-id]))
      (log/warn :message (format "toolbar %s not found" toolbar-id)))))

(declare refresh)

(defn- toolbar-control
  [scene menu-item handler-ctx localization evaluation-context]
  (let [separator? (= :separator (:label menu-item))
        opts (handler/options handler-ctx evaluation-context)]
    (cond
      separator?
      (doto (Separator. Orientation/VERTICAL)
        (add-style! "separator"))

      opts
      (let [hbox (doto (HBox.)
                   (add-style! "cell"))
            cb (doto (ChoiceBox.)
                 (.setConverter (DefoldStringConverter. (comp localization :label))))]
        (.setAll (.getItems cb) ^Collection opts)
        (observe (.valueProperty cb) (fn [_this _old new]
                                       (when (and new (not *programmatic-selection*))
                                         (let [command-contexts (contexts scene true)]
                                           (execute-command command-contexts (:command new) (:user-data new))))))
        (.add (.getChildren hbox) (icons/get-image-view (:icon menu-item) 16))
        (.add (.getChildren hbox) cb)
        hbox)

      :else
      (let [{:keys [graphic-fn label icon tooltip more]} menu-item
            label (or (handler/label handler-ctx evaluation-context) label)
            button (doto (ToggleButton.)
                     (localization/localize! localization label)
                     (tooltip! tooltip localization))]
        (cond
          graphic-fn
          ;; TODO: Ideally, we'd create the graphic once and simply assign it here.
          ;; Trouble is, the toolbar takes ownership of the Node tree, so the graphic
          ;; disappears from the toolbars of subsequent tabs. For now, we generate
          ;; instances for each tab.
          (.setGraphic button (graphic-fn))

          icon
          (.setGraphic button (icons/get-image-view icon 16)))

        (when-let [command (:command menu-item)]
          (on-action! button (fn [_event]
                               (execute-command (contexts scene true) command (:user-data menu-item)))))

        (if more
          (let [{:keys [id command]} more
                group (doto (HBox.)
                        (add-style! "button-group"))
                icon (icons/get-image-view "icons/32/Icons_S_05_arrowdown.png" 18)
                more-button (doto (Button.)
                              (.setGraphic icon)
                              (add-style! "more-button")
                              (on-action! (fn [_event]
                                            (execute-command (contexts scene true) command (:user-data menu-item)))))]
            (.add (.getChildren group) button)
            (.add (.getChildren group) more-button)
            (when id (.setId more-button (name id)))
            (observe (.selectedProperty button)
                     (fn [_observable _old-val new-val]
                       (if new-val
                         (add-style! group "active")
                         (remove-style! group "active"))))
            group)
          button)))))

(defn- refresh-toolbar [td command-contexts localization evaluation-context]
 (let [menu (handler/realize-menu (:menu-id td))
       ^Pane control (:control td)
       scene (.getScene control)]
   (when (and (some? scene)
              (or (not= menu (user-data control ::menu))
                  (not= command-contexts (user-data control ::command-contexts))))
     (.clear (.getChildren control))
     (user-data! control ::menu menu)
     (user-data! control ::command-contexts command-contexts)
     (let [children (doall
                      (for [menu-item menu
                            :let [command (:command menu-item)
                                  user-data (:user-data menu-item)
                                  separator? (= :separator (:label menu-item))
                                  handler-ctx (handler/active command command-contexts user-data evaluation-context)]
                            :when (or separator? handler-ctx)]
                        (let [^Control child (toolbar-control scene menu-item handler-ctx localization evaluation-context)]
                          (when command
                            (user-data! child ::command command))
                          (user-data! child ::menu-user-data user-data)
                          child)))
           children (cond-> children
                      (instance? Separator (last children)) butlast
                      (instance? Separator (first children)) rest)]
       (doseq [child children]
         (.add (.getChildren control) child))))))

(defn- refresh-toolbar-state [^Pane toolbar command-contexts evaluation-context]
  (let [nodes (.getChildren toolbar)]
    (doseq [^Node n nodes
            :let [command (user-data n ::command)
                  user-data (user-data n ::menu-user-data)
                  handler-ctx (handler/active command command-contexts user-data evaluation-context)]]
      (disable! n (not (handler/enabled? handler-ctx evaluation-context)))
      (when (instance? ToggleButton n)
        (if (handler/state handler-ctx evaluation-context)
          (.setSelected ^Toggle n true)
          (.setSelected ^Toggle n false)))
      (when (instance? HBox n)
        (let [^HBox box n
              state (handler/state handler-ctx evaluation-context)
              second-child (.get (.getChildren box) 1)]
          (cond
            (instance? ChoiceBox second-child)
            (let [^ChoiceBox cb second-child]
              (when (not (.isShowing cb))
                (let [items (.getItems cb)
                      opts (vec items)
                      new-opts (vec (handler/options handler-ctx evaluation-context))]
                  (when (not= opts new-opts)
                    (.setAll items ^Collection new-opts)))
                (let [selection-model (.getSelectionModel cb)
                      item (.getSelectedItem selection-model)]
                  (when (not= item state)
                    (binding [*programmatic-selection* true]
                      (.select selection-model state))))))

            :else
            (let [toggle-button (.get (.getChildren box) 0)]
              (if (handler/state handler-ctx evaluation-context)
                (.setSelected ^Toggle toggle-button true)
                (.setSelected ^Toggle toggle-button false)))))))))

(defn- window-parents [^Window window]
  (when-let [parent (condp instance? window
                      Stage (let [^Stage stage window] (.getOwner stage))
                      PopupWindow (let [^PopupWindow popup window] (.getOwnerWindow popup))
                      nil)]
    [parent]))

(defn- scene-chain [^Scene leaf]
  (if-let [leaf-window (.getWindow leaf)]
    (reduce (fn [scenes ^Window window] (conj scenes (.getScene window)))
            []
            (tree-seq window-parents window-parents leaf-window))
    [leaf]))

(defn- visible-command-contexts [^Scene scene evaluation-context]
  (let [parent-scenes (rest (scene-chain scene))]
    (apply concat (contexts scene true evaluation-context)
           (map (fn [parent-scene]
                  (filter #(= :global (:name %)) (contexts parent-scene true evaluation-context)))
                parent-scenes))))

(defn- current-command-contexts [^Scene scene evaluation-context]
  (contexts scene true evaluation-context))

(defn- refresh-menus!
  [^Scene scene keymap localization evaluation-context]
  (let [visible-command-contexts (visible-command-contexts scene evaluation-context)
        current-command-contexts (current-command-contexts scene evaluation-context)
        root (.getRoot scene)]
    (when-let [md (user-data root ::menubar)]
      (let [^MenuBar menu-bar (:control md)
            menu (cond-> (handler/realize-menu (:menu-id md))
                         (os/is-mac-os?)
                         (menu-data-without-icons))]
        (cond
          (refresh-menubar? menu-bar menu visible-command-contexts keymap)
          (refresh-menubar! menu-bar menu visible-command-contexts keymap localization evaluation-context)

          (refresh-menubar-items?)
          (refresh-menubar-items! menu-bar menu visible-command-contexts keymap localization evaluation-context))

        (refresh-menubar-state menu-bar current-command-contexts evaluation-context)))))

(defn- refresh-toolbars!
  [^Scene scene localization evaluation-context]
  (let [visible-command-contexts (visible-command-contexts scene evaluation-context)
        current-command-contexts (current-command-contexts scene evaluation-context)
        root (.getRoot scene)
        app-view (-> current-command-contexts first :env :app-view)
        active-tab (g/maybe-node-value app-view :active-tab evaluation-context)]
    (doseq [td (vals (user-data root ::toolbars))]
      (let [control (:control td)]
        (when active-tab
          (visible! control (nodes-along-path? control (.getContent ^Tab active-tab) root)))
        (refresh-toolbar td visible-command-contexts localization evaluation-context)
        (refresh-toolbar-state (:control td) current-command-contexts evaluation-context)))))

(defn- refresh-accelerators! [scene keymap]
  (when-not (identical? keymap (user-data scene ::accelerators))
    (user-data! scene ::accelerators keymap)
    (keymap/install! keymap scene execute-accelerator-commands)))

(defn refresh
  ([^Scene scene]
   (g/with-auto-or-fake-evaluation-context evaluation-context
     (refresh scene evaluation-context)))
  ([^Scene scene evaluation-context]
   (let [keymap (or (user-data scene :keymap) keymap/empty)
         localization (user-data scene :localization)]
     (assert (some? localization))
     (refresh-accelerators! scene keymap)
     (refresh-menus! scene keymap localization evaluation-context)
     (refresh-toolbars! scene localization evaluation-context))))

(defn render-progress-bar! [progress ^ProgressBar bar]
  (.setProgress
    bar
    (if-let [fraction (when-not (progress/cancelled? progress)
                        (progress/fraction progress))]
      (double fraction)
      -1.0)))

(defn render-progress-message! [progress ^Label label localization]
  (localization/localize! label localization (progress/message progress)))

(defn render-progress-percentage! [progress ^Label label localization]
  (localization/localize!
    label
    localization
    (if (progress/cancelled? progress)
      (localization/message "progress.aborting")
      (if-some [percentage (progress/percentage progress)]
        (localization/message "progress.percentage" {"percentage" percentage})
        localization/empty-message))))

(defmacro with-progress [bindings & body]
  `(let ~bindings
     (try
       ~@body
       (finally
         ((second ~bindings) progress/done)))))

(defn- set-scene-disable! [^Scene scene disable?]
  (when-some [root (.getRoot scene)]
    (.setDisable root disable?)
    ;; Menus are unaffected by the disabled root, so we must explicitly disable them.
    (doseq [^Menu menu (mapcat #(.getMenus ^MenuBar %) (.lookupAll root "MenuBar"))]
      (.setDisable menu disable?))))

(defn- push-disable-ui! [disable-ui-focus-owner-stack]
  (assert (on-ui-thread?))
  (let [scene (some-> (main-stage) .getScene)
        focus-owner (some-> scene focus-owner)]
    (when (and (some? scene)
               (empty? disable-ui-focus-owner-stack))
      (set-scene-disable! scene true))
    (conj disable-ui-focus-owner-stack focus-owner)))

(defn- pop-disable-ui! [disable-ui-focus-owner-stack]
  (assert (on-ui-thread?))
  (when (= 1 (count disable-ui-focus-owner-stack))
    (when-some [scene (some-> (main-stage) .getScene)]
      (set-scene-disable! scene false)
      (when-some [^Node focus-owner (peek disable-ui-focus-owner-stack)]
        (.requestFocus focus-owner))))
  (pop disable-ui-focus-owner-stack))

(def ^:private disable-ui-focus-owner-stack-volatile (volatile! []))

(defn ui-disabled? []
  (run-now
    (not (empty? (deref disable-ui-focus-owner-stack-volatile)))))

(defn disable-ui! []
  (run-later
    (vswap! disable-ui-focus-owner-stack-volatile push-disable-ui!)))

(defn enable-ui! []
  (run-later
    (vswap! disable-ui-focus-owner-stack-volatile pop-disable-ui!)))

(defn handle
  [f]
  (reify EventHandler
    (handle [this event] (f event))))

(defonce/protocol Future
  (cancel [this])
  (restart [this]))

(extend-type Timeline
  Future
  (cancel [this] (.stop this))
  (restart [this] (.playFromStart this)))

(defn ->future [delay run-fn]
  (let [^EventHandler handler (event-handler e (run-later (run-fn)))
        ^"[Ljavafx.animation.KeyValue;" values (into-array KeyValue [])]
    ; TODO - fix reflection ctor warning
    (doto (Timeline. 60 (into-array KeyFrame [(KeyFrame. ^Duration (Duration/seconds delay) handler values)]))
      (.play))))

(defn ->timer
  ([name tick-fn]
   (->timer nil name tick-fn))
  ([fps name tick-fn]
   (let [start (System/nanoTime)
         last (atom start)
         interval (if fps
                    (long (* 1e9 (/ 1 (double fps))))
                    0)]
     {:last last
      :timer (proxy [AnimationTimer] []
               (handle [^long now]
                 (profiler/profile "timer" name
                   (let [elapsed (- now start)
                         delta (- now (long @last))]
                     (when (or (zero? interval) (> delta interval))
                       (run-later
                         (try
                           (tick-fn this (* elapsed 1e-9) (/ delta 1e9))
                           (reset! last (if (zero? interval)
                                          now
                                          (- now (- delta interval))))
                           (catch Throwable t
                             (.stop ^AnimationTimer this)
                             (swap! stopped-timers conj this)
                             (error-reporting/report-exception! t)))))))))})))

(defn timer-start! [timer]
  (.start ^AnimationTimer (:timer timer)))

(defn timer-stop! [timer]
  (.stop ^AnimationTimer (:timer timer)))

(defn enable-stopped-timers!
  "Re-enables any AnimationTimers that were stopped due to exceptions."
  []
  (doseq [timer @stopped-timers]
    (.start ^AnimationTimer timer))
  (reset! stopped-timers #{})
  nil)

(defn anim! [^double duration anim-fn end-fn]
  (let [duration (long (* 1e9 duration))
        start (System/nanoTime)
        end (+ start (long duration))]
    (doto (proxy [AnimationTimer] []
            (handle [^long now]
              (run-later
                (if (< now end)
                  (let [t (/ (double (- now start)) duration)]
                    (try
                      (anim-fn t)
                      (catch Throwable t
                        (.stop ^AnimationTimer this)
                        (swap! stopped-timers conj this)
                        (error-reporting/report-exception! t))))
                  (try
                    (end-fn)
                    (catch Throwable t
                      (error-reporting/report-exception! t))
                    (finally
                      (.stop ^AnimationTimer this)))))))
      (.start))))

(defn anim-stop! [^AnimationTimer anim]
  (.stop anim))

(defn- chain-handler [new-handler-fn ^EventHandler existing-handler]
  (event-handler e
                 (new-handler-fn e)
                 (when existing-handler
                   (.handle existing-handler e))))

(defonce/protocol CloseRequestable
  (on-closing [this])
  (on-closing! [this f]))

(extend-type javafx.stage.Stage
  CloseRequestable
  (on-closing [this] (.getOnCloseRequest this))
  (on-closing! [this f]
    (.setOnCloseRequest this (chain-handler
                              (fn [^Event e]
                                (when-not (f e)
                                  (.consume e)))
                              (on-closing this)))))

(defonce/protocol Closeable
  (on-closed [this])
  (on-closed! [this f]))

(extend-protocol Closeable
  javafx.scene.control.Tab
  (on-closed [this] (.getOnClosed this))
  (on-closed! [this f] (.setOnClosed this (chain-handler f (on-closed this))))

  javafx.stage.Window
  (on-closed [this] (.getOnHidden this))
  (on-closed! [this f] (.setOnHidden this (chain-handler f (on-closed this)))))

(defn timer-stop-on-closed!
  [closeable timer]
  (on-closed! closeable (fn [_] (timer-stop! timer))))

(defn- show-dialog-stage [^Stage stage localization show-fn]
  (.setOnShown stage (event-handler _ (slog/smoke-log "show-dialog")))
  (user-data! (.getScene stage) :localization localization)
  (if (and (os/is-mac-os?)
           (= (.getOwner stage) (main-stage)))
    (let [scene (.getScene stage)
          root-pane ^Pane (.getRoot scene)
          menu-bar (doto (MenuBar.)
                     (.setUseSystemMenuBar true))
          refresh-timer (->timer 3 "refresh-dialog-ui" (fn [_ _ _] (refresh scene)))]
      (.. root-pane getChildren (add menu-bar))
      (register-menubar scene menu-bar (main-menu-id))
      (refresh scene)
      (timer-start! refresh-timer)
      (timer-stop-on-closed! stage refresh-timer)
      (show-fn stage))
    (show-fn stage)))

(defn show! [^Stage stage localization] (show-dialog-stage stage localization (fn [^Stage stage] (.show stage))))

(defn drag-internal? [^DragEvent e]
  (some? (.getGestureSource e)))

(defn register-tab-toolbar [^Tab tab toolbar-css-selector menu-id]
  (let [scene (-> tab .getTabPane .getScene)
        context-node (.getContent tab)]
    (when (.lookup context-node toolbar-css-selector)
      (register-toolbar scene context-node toolbar-css-selector menu-id)
      (on-closed! tab (fn [_event]
                        (unregister-toolbar scene context-node toolbar-css-selector))))))

(defn parent-tab-pane
  "Returns the closest TabPane at or above the Node in the scene hierarchy, or
  nil if the Node is not a TabPane or under a TabPane."
  ^TabPane [^Node node]
  (closest-node-of-type TabPane node))

(defn tab-pane-parent
  "Returns the parent Node of a TabPane, or nil if the TabPane is not in a scene.
  TabPanes are wrapped in a skin Node, so its parent is one additional step up."
  ^Node [^TabPane tab-pane]
  (some-> (.getParent tab-pane) .getParent))

(defn selected-tab
  ^Tab [^TabPane tab-pane]
  (.. tab-pane getSelectionModel getSelectedItem))

(defn select-tab! [^Tab tab]
  (let [tab-pane (.getTabPane tab)
        selection-model (.getSelectionModel tab-pane)]
    (.select selection-model tab)))

(defn inside-hidden-tab? [^Node node]
  (let [tab-content-area (closest-node-with-style "tab-content-area" node)]
    (and (some? tab-content-area)
         (not= tab-content-area
               (some-> tab-content-area
                       .getParent
                       selected-tab
                       .getContent
                       .getParent)))))

;; NOTE: Running Desktop methods on JavaFX application thread locks
;; application on Linux, so we do it on a new thread.

(defonce ^Desktop desktop (when (Desktop/isDesktopSupported) (Desktop/getDesktop)))

(defn as-url
  ^URI [url]
  (if (instance? URI url) url (URI. url)))

(defn open-url
  [url]
  (if (some-> desktop (.isSupported Desktop$Action/BROWSE))
    (do
      (.start (Thread. #(.browse desktop (as-url url))))
      true)
    (do
      (log/warn :message (str "Cannot open browser." (when (os/is-linux?) " Installing gvfs may fix this.")))
      false)))

(defn open-file
  ([^File file]
   (open-file file nil))
  ([^File file on-error-fn]
   (if (some-> desktop (.isSupported Desktop$Action/OPEN))
     (do
       (.start (Thread. (fn []
                          (try
                            (.open desktop file)
                            (catch IOException e
                              (if on-error-fn
                                (on-error-fn (.getMessage e))
                                (throw e)))))))
       true)
     (do
       (log/warn :message "Cannot open file handler")
       false))))

(defn- make-path-data
  ^String [^double col ^double row outlines]
  (string/join " "
               (map (fn [outline]
                      (str "M" (string/join " L"
                                            (map (fn [^double x ^double y]
                                                   (str (math/round-with-precision (* x col) 0.1)
                                                        ","
                                                        (math/round-with-precision (* y row) 0.1)))
                                                 (take-nth 2 outline)
                                                 (take-nth 2 (drop 1 outline)))) " Z"))
                    outlines)))

(defn make-defold-logo-paths [^double width ^double height]
  (let [col (/ width 6.0)
        row (/ height 10.0)]
    [(doto (SVGPath.)
       (add-style! "left")
       (.setContent (make-path-data col row [[0,4 1,3 1,1 2,0 2,2 4,0 4,2 3,1 3,3 2,4 2,6 1,7 2,8 1,9 1,5 0,6] [3,5 4,4 4,6 3,7]])))
     (doto (SVGPath.)
       (add-style! "right")
       (.setContent (make-path-data col row [[3,1 2,2 2,0 3,1 4,2 4,0 5,1 5,3 6,4 6,6 5,5 5,9 4,8 5,7 4,6 4,4 3,3] [3,5 3,7 2,6 2,4]])))
     (doto (SVGPath.)
       (add-style! "bottom")
       (.setContent (make-path-data col row [[0,6 1,5 1,7 2,6 3,7 4,6 5,7 5,5 6,6 5,7 4,8 5,9 4,10 3,9 2,10 1,9 2,8] [3,5 2,4 3,3 4,4]])))]))

(defn string->menu-item
  ^MenuItem [^String str]
  (doto (MenuItem.)
    (.setText str)))

(defn show-simple-context-menu!
  [menu-item-fn item-action-fn items ^Node anchor-node ^Point2D offset]
  (let [handle-action! (fn [^Event event]
                         (let [menu-item (.getTarget event)
                               item (user-data menu-item ::item)]
                           (item-action-fn item)))
        menu-items (map (fn [item]
                          (doto (menu-item-fn item)
                            (user-data! ::item item)
                            (on-action! handle-action!)))
                        items)
        context-menu (doto (make-context-menu menu-items)
                       (on-closed! (fn [_]
                                     (item-action-fn nil))))
        hide-event-handler (event-handler event (.hide context-menu))]
    (.addEventFilter anchor-node MouseEvent/MOUSE_PRESSED hide-event-handler)
    (on-closed! context-menu (fn [_]
                               (.removeEventFilter anchor-node MouseEvent/MOUSE_PRESSED hide-event-handler)
                               (item-action-fn nil)))
    (.show context-menu anchor-node (.getX offset) (.getY offset))))

(defn show-simple-context-menu-at-mouse!
  [menu-item-fn item-action-fn items ^MouseEvent mouse-event]
  (let [anchor-node (.getTarget mouse-event)
        offset (Point2D. (.getScreenX mouse-event) (.getScreenY mouse-event))]
    (show-simple-context-menu! menu-item-fn item-action-fn items anchor-node offset)))

(defn select-all-on-click! [^TextInputControl t]
  (doto t
    ;; Filter is necessary because the listener will be called after the text field has received focus, i.e. too late
    (.addEventFilter MouseEvent/MOUSE_PRESSED (event-handler e
                                                (when (not (focus? t))
                                                  (.deselect t)
                                                  (user-data! t ::selection-at-focus true))))
    ;; Filter is necessary because the TextArea captures the event
    (.addEventFilter MouseEvent/MOUSE_RELEASED (event-handler e
                                                 (when (user-data t ::selection-at-focus)
                                                   (when (string/blank? (.getSelectedText t))
                                                     (.consume e)
                                                     (run-later (.selectAll t))))
                                                 (user-data! t ::selection-at-focus nil)))))


(defmulti customize! (fn [control _ _] (class control)))

(defmethod customize! TextField [^TextField t update-fn cancel-fn]
  (doto t
    (GridPane/setHgrow Priority/ALWAYS)
    (on-action! update-fn)
    (on-cancel! cancel-fn)
    (auto-commit! update-fn)
    (select-all-on-click!)))

(defmethod customize! TextArea [^TextArea t update-fn cancel-fn]
  (doto t
    (GridPane/setHgrow Priority/ALWAYS)
    (on-action! update-fn)
    (on-cancel! cancel-fn)
    (auto-commit! update-fn)
    (select-all-on-click!)))

(defn set-cursor
  [^Node node ^Cursor cursor-type]
  ;; The cursor refresh appears buggy at the moment.
  ;; Calling setCursor with DISAPPEAR before setting the cursor forces it to refresh.
  (when (not= cursor-type (.getCursor node))
    (.setCursor node Cursor/DISAPPEAR)
    (.setCursor node cursor-type)))

(defn force-layout! [^Node node]
  ;; Force layout pass to ensure the proper dimensions can be queried from the
  ;; node. This process is recursive.
  (NodeHelper/layoutNodeForPrinting node))

(defn force-scene-layout! [^Scene scene]
  (force-layout! (.getRoot scene)))

(defn node-timer!
  "Installs a timer on a node

  The supplied function will only be invoked if the node is a part of the
  rendered tree (i.e. it and its parents are visible, the node is on a showing
  window)

  Args:
    node    target Node
    fps     timer fps
    name    timer name, a string
    f       0-arg function"
  [^Node node fps name f]
  (let [timer (->timer fps name (fn [_ _ _] (f)))
        tree-visible-property (NodeHelper/treeVisibleProperty node)
        tree-showing-property (-> node
                                  (.sceneProperty)
                                  (.flatMap Scene/.windowProperty)
                                  (.flatMap Window/.showingProperty)
                                  (.orElse false))
        running-property (Bindings/createBooleanBinding
                           #(and (.getValue tree-showing-property)
                                 (.get tree-visible-property))
                           (into-array Observable [tree-showing-property tree-visible-property]))
        ^ChangeListener on-running-changed (fn [_ _ tree-visible]
                                             (if tree-visible
                                               (do (f) (timer-start! timer))
                                               (timer-stop! timer)))
        key (Object.)
        node-properties (.getProperties node)]
    (when (.get running-property)
      (.changed on-running-changed running-property false true))
    (.addListener running-property on-running-changed)
    ;; Bindings are weakly-referenced, so we need to preserve a ref to it on a
    ;; node so that the listener doesn't disappear suddenly in cases where we
    ;; don't keep the dispose-fn referenced
    (.put node-properties key running-property)
    (fn dispose-node-timer! []
      (.remove node-properties key)
      (.removeListener running-property on-running-changed)
      (timer-stop! timer))))
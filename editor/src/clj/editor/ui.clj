(ns editor.ui
  (:require
   [clojure.java.io :as io]
   [clojure.set :as set]
   [clojure.string :as string]
   [editor.error-reporting :as error-reporting]
   [editor.handler :as handler]
   [editor.jfx :as jfx]
   [editor.progress :as progress]
   [editor.menu :as menu]
   [editor.ui.tree-view-hack :as tree-view-hack]
   [internal.util :as util]
   [service.log :as log]
   [util.profiler :as profiler])
  (:import
   [com.defold.control LongField]
   [com.defold.control ListCell]
   [com.defold.control TreeCell]
   [java.awt Desktop]
   [java.net URI]
   [javafx.animation AnimationTimer Timeline KeyFrame KeyValue]
   [javafx.application Platform]
   [javafx.beans.value ChangeListener ObservableValue]
   [javafx.collections ObservableList ListChangeListener ListChangeListener$Change]
   [javafx.css Styleable]
   [javafx.event ActionEvent EventHandler WeakEventHandler Event]
   [javafx.fxml FXMLLoader]
   [javafx.geometry Orientation]
   [javafx.scene Parent Node Scene Group]
   [javafx.scene.control ButtonBase CheckBox ChoiceBox ColorPicker ComboBox ComboBoxBase Control ContextMenu Separator SeparatorMenuItem Label Labeled ListView ToggleButton TextInputControl TreeView TreeItem Toggle Menu MenuBar MenuItem MultipleSelectionModel CheckMenuItem ProgressBar TabPane Tab TextField Tooltip SelectionMode SelectionModel]
   [javafx.scene.input Clipboard KeyCombination ContextMenuEvent MouseEvent DragEvent KeyEvent KeyCode]
   [javafx.scene.image Image]
   [javafx.scene.layout Region]
   [javafx.scene.layout AnchorPane Pane HBox]
   [javafx.stage DirectoryChooser FileChooser FileChooser$ExtensionFilter]
   [javafx.stage Stage Modality Window]
   [javafx.util Callback Duration StringConverter]))

(set! *warn-on-reflection* true)

;; These two lines initialize JavaFX and OpenGL when we're generating
;; API docs
(import com.sun.javafx.application.PlatformImpl)
(PlatformImpl/startup (constantly nil))

(defonce ^:dynamic *menu-key-combos* (atom #{}))
(defonce ^:dynamic *main-stage* (atom nil))

(defprotocol Text
  (text ^String [this])
  (text! [this ^String val]))

(defprotocol HasValue
  (value [this])
  (value! [this val]))

(defprotocol HasUserData
  (user-data [this key])
  (user-data! [this key val]))

(defprotocol Editable
  (editable [this])
  (editable! [this val])
  (on-edit! [this fn]))

(def application-icon-image (Image. (io/input-stream (io/resource "logo_blue.png"))))

(defn make-stage
  ^Stage []
  (doto (Stage.)
    (.. getIcons (add application-icon-image))))

; NOTE: This one might change from welcome to actual project window
(defn set-main-stage [main-stage]
  (reset! *main-stage* main-stage))

(defn ^Stage main-stage []
  @*main-stage*)

(defn ^Scene main-scene []
  (.. (main-stage) (getScene)))

(defn ^Node main-root []
  (.. (main-scene) (getRoot)))

(defn choose-file [title ^String ext-descr exts]
  (let [chooser (FileChooser.)
        ext-array (into-array exts)]
    (.setTitle chooser title)
    (.add (.getExtensionFilters chooser) (FileChooser$ExtensionFilter. ext-descr ^"[Ljava.lang.String;" ext-array))
    (let [file (.showOpenDialog chooser nil)]
      (if file (.getAbsolutePath file)))))

(defn choose-directory
  ([title] (choose-directory title @*main-stage*))
  ([title parent]
    (let [chooser (DirectoryChooser.)]
      (.setTitle chooser title)
      (let [file (.showDialog chooser parent)]
        (when file (.getAbsolutePath file))))))

(defn collect-controls [^Parent root keys]
  (let [controls (zipmap (map keyword keys) (map #(.lookup root (str "#" %)) keys))
        missing (->> controls
                  (filter (fn [[k v]] (when (nil? v) k)))
                  (map first))]
    (when (seq missing)
      (throw (Exception. (format "controls %s are missing" missing))))
    controls))

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

(defn observe [^ObservableValue observable listen-fn]
  (.addListener observable (reify ChangeListener
                        (changed [this observable old new]
                          (listen-fn observable old new)))))

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

(defmacro observe-selection
  "Helper macro that lets you observe selection changes in a generic fashion.
  Takes a Node that has a getSelectionModel method and a function that takes
  the reporting Node and a vector with the selected items as its arguments.

  This is a macro because JavaFX does not have a common interface for classes
  that feature a getSelectionModel method. To avoid reflection warnings, tag
  the node argument with type metadata.

  Supports both single and multi-selection. In both cases the selected items
  will be provided in a vector."
  [node listen-fn]
  `(let [selection-owner# ~node
         selection-listener# ~listen-fn
         selection-model# (.getSelectionModel selection-owner#)]
     (condp instance? selection-model#
       MultipleSelectionModel
       (observe-list selection-owner#
                     (.getSelectedItems ^MultipleSelectionModel selection-model#)
                     (fn [_# selected-items#]
                       (selection-listener# selection-owner# selected-items#)))

       SelectionModel
       (observe (.selectedItemProperty ^SelectionModel selection-model#)
                (fn [_# _# selected-item#]
                  (selection-listener# selection-owner# [selected-item#]))))))

(defn remove-list-observers
  [^Node node ^ObservableList observable]
  (let [listeners (user-data node ::list-listeners)]
    (user-data! node ::list-listeners [])
    (doseq [^ListChangeListener l listeners]
      (.removeListener observable l))))

(defn do-run-now [f]
  (if (Platform/isFxApplicationThread)
    (f)
    (let [p (promise)]
      (Platform/runLater
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
  `(Platform/runLater
    (fn [] ~@body)))

(defmacro event-handler [event & body]
  `(reify EventHandler
     (handle [~'this ~event]
       ~@body)))

(defmacro change-listener [observable old-val new-val & body]
  `(reify ChangeListener
     (changed [~'this ~observable ~old-val ~new-val]
       ~@body)))

(defn scene [^Node node]
  (.getScene node))

(defn add-styles! [^Styleable node classes]
  (let [styles (.getStyleClass node)
        existing (into #{} styles)
        new (filter (complement existing) classes)]
    (when-not (empty? new)
      (.addAll styles ^java.util.Collection new))))

(defn add-style! [^Styleable node ^String class]
  (add-styles! node [class]))

(defn remove-styles! [^Styleable node ^java.util.Collection classes]
  (let [styles (.getStyleClass node)
        existing (into #{} styles)
        old (filter existing classes)]
    (when-not (empty? old)
      (.removeAll styles ^java.util.Collection old))))

(defn remove-style! [^Styleable node ^String class]
  (remove-styles! node [class]))

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


(defn restyle-tabs! [^TabPane tab-pane]
  (let [tabs (seq (.getTabs tab-pane))]
    (doseq [tab tabs]
      (remove-styles! tab ["first-tab" "last-tab"]))
    (when-let [first-tab (first tabs)]
      (add-style! first-tab "first-tab"))
    (when-let [last-tab (last tabs)]
      (add-style! last-tab "last-tab"))))

(defn reload-root-styles! []
  (when-let [scene (.getScene ^Stage (main-stage))]
    (let [root ^Parent (.getRoot scene)
          styles (vec (.getStylesheets root))]
      (.forget (com.sun.javafx.css.StyleManager/getInstance) scene)
      (.setAll (.getStylesheets root) ^java.util.Collection styles))))

(defn visible! [^Node node v]
  (.setVisible node v))

(defn managed! [^Node node m]
  (.setManaged node m))

(defn disable! [^Node node d]
  (.setDisable node d))

(defn window [^Scene scene]
  (.getWindow scene))

(defn close! [^Stage window]
  (.close window))

(defn title! [^Stage window t]
  (.setTitle window t))

(defn tooltip! [^Control ctrl tip]
  (.setTooltip ctrl (when tip (Tooltip. tip))))

(defn show! [^Stage stage]
  (.show stage))

(defn show-and-wait! [^Stage stage]
  (.showAndWait stage))

(defn show-and-wait-throwing!
  "Like show-and wait!, but will immediately close the stage if an
  exception is thrown from the nested event loop. The exception can
  then be caught at the call site."
  [^Stage stage]
  (when (nil? stage)
    (throw (IllegalArgumentException. "stage cannot be nil")))
  (let [prev-exception-handler (Thread/getDefaultUncaughtExceptionHandler)
        thrown-exception (volatile! nil)]
    (Thread/setDefaultUncaughtExceptionHandler (reify Thread$UncaughtExceptionHandler
                                                 (uncaughtException [_ _ exception]
                                                   (vreset! thrown-exception exception)
                                                   (close! stage))))
    (let [result (try
                   (.showAndWait stage)
                   (finally
                     (Thread/setDefaultUncaughtExceptionHandler prev-exception-handler)))]
      (if-let [exception @thrown-exception]
        (throw exception)
        result))))

(defn request-focus! [^Node node]
  (.requestFocus node))

(defn focus? [^Node node]
  (.isFocused node))

(defn on-double! [^Node node fn]
  (.setOnMouseClicked node (event-handler e (when (= 2 (.getClickCount ^MouseEvent e))
                                              (fn e)))))

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

(defn load-fxml [path]
  (let [root ^Parent (FXMLLoader/load (io/resource path))
        css (io/file "editor.css")]
    (when (and (.exists css) (seq (.getStylesheets root)))
      (.setAll (.getStylesheets root) ^java.util.Collection (vec [(str (.toURI css))])))
    root))

(extend-type Window
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
  HasUserData
  (user-data [this key] (get (.getUserData this) key))
  (user-data! [this key val] (.setUserData this (assoc (or (.getUserData this) {}) key val))))

(extend-type Tab
  HasUserData
  (user-data [this key] (get (.getUserData this) key))
  (user-data! [this key val] (.setUserData this (assoc (or (.getUserData this) {}) key val))))

(defprotocol HasAction
  (on-action! [this fn]))

(defprotocol HasChildren
  (children! [this c])
  (add-child! [this c]))

(defprotocol CollectionView
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
  (value! [this val] (.setText this (str val))))

(extend-type TextInputControl
  HasValue
  (value [this] (text this))
  (value! [this val] (text! this val))
  Text
  (text [this] (.getText this))
  ; TODO: This is hack to reduce the cpu usage due to bug DEFEDIT-131
  (text! [this val] (when-not (= val (.getText this))
                      (.setText this val)
                      (when (.isFocused this)
                        (.end this)
                        (.selectAll this))))
  Editable
  (editable [this] (.isEditable this))
  (editable! [this val] (.setEditable this val))
  (on-edit! [this f] (observe (.textProperty this) (fn [this old new] (f old new)))))

(extend-type ComboBoxBase
  Editable
  (editable [this] (.isEditable this))
  (editable! [this val] (.setEditable this val))
  (on-edit! [this f] (observe (.valueProperty this) (fn [this old new] (f old new)))))

(extend-type CheckBox
  HasValue
  (value [this] (.isSelected this))
  (value! [this val] (.setSelected this val))
  Editable
  (editable [this] (not (.isDisabled this)))
  (editable! [this val] (.setDisable this (not val)))
  (on-edit! [this f] (observe (.selectedProperty this) (fn [this old new] (f old new)))))

(extend-type ColorPicker
  HasValue
  (value [this] (.getValue this))
  (value! [this val] (.setValue this val)))

(extend-type TextField
  HasAction
  (on-action! [this fn] (.setOnAction this (event-handler e (fn e))))
  Text
  (text [this] (.getText this))
  (text! [this val] (.setText this val)))

(extend-type Labeled
  Text
  (text [this] (.getText this))
  (text! [this val] (.setText this val)))

(extend-type Label
  Text
  (text [this] (.getText this))
  (text! [this val] (.setText this val)))

(extend-type Pane
  HasChildren
  (children! [this c]
    (doto
      (.getChildren this)
      (.clear)
      (.addAll ^"[Ljavafx.scene.Node;" (into-array Node c))))
  (add-child! [this c]
    (-> this (.getChildren) (.add c))))

(extend-type Group
  HasChildren
  (children! [this c]
    (doto
      (.getChildren this)
      (.clear)
      (.addAll ^"[Ljavafx.scene.Node;" (into-array Node c))))
  (add-child! [this c]
    (-> this (.getChildren) (.add c))))

(defn- make-style-applier []
  (let [applied-style-classes (volatile! #{})]
    (fn [^Styleable styleable style-classes]
      (when-not (set? style-classes)
        (throw (IllegalArgumentException. "style-classes must be a set")))
      (let [current @applied-style-classes
            removed ^java.util.Collection (set/difference current style-classes)
            added  ^java.util.Collection (set/difference style-classes current)]
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
              (proxy-super setText (:text render-data))
              (when-let [icon (:icon render-data)]
                (proxy-super setGraphic (jfx/get-image-view icon 16)))))
          (proxy-super setTooltip (:tooltip render-data)))))))

(defn- make-list-cell-factory [render-fn]
  (reify Callback (call ^ListCell [this view] (make-list-cell render-fn))))

(defn- make-tree-cell [render-fn]
  (let [cell (proxy [TreeCell] []
               (updateItem [resource empty]
                 (let [^TreeCell this this
                       render-data (and resource (render-fn resource))]
                   (proxy-super updateItem resource empty)
                   (update-tree-cell-style! this)
                   (if empty
                     (do
                       (proxy-super setText nil)
                       (proxy-super setGraphic nil))
                     (do
                       (when-let [text (:text render-data)]
                         (proxy-super setText text))
                       (when-let [icon (:icon render-data)]
                         (proxy-super setGraphic (jfx/get-image-view icon 16))))))))]
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
  (selection [this] (when-let [item (.getSelectedItem (.getSelectionModel this))] [item]))
  (select! [this item] (doto (.getSelectionModel this)
                         (.select item)))
  (select-index! [this index] (doto (.getSelectionModel this)
                                (.select (int index))))
  (selection-mode! [this mode] (when (= :multiple mode)
                                 (throw (UnsupportedOperationException. "ComboBox only has single selection"))))
  (items [this] (.getItems this))
  (items! [this ^java.util.Collection items] (let [l (.getItems this)]
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
  (selection [this] (when-let [items (.getSelectedItems (.getSelectionModel this))] items))
  (select! [this item] (doto (.getSelectionModel this)
                         (.select item)))
  (select-index! [this index] (doto (.getSelectionModel this)
                                (.select (int index))))
  (selection-mode! [this mode] (let [^SelectionMode mode (selection-mode mode)]
                                 (.setSelectionMode (.getSelectionModel this) mode)))
  (items [this] (.getItems this))
  (items! [this ^java.util.Collection items] (let [l (.getItems this)]
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
  (doto (.getSelectionModel tree-view)
    (tree-view-hack/subvert-broken-selection-model-optimization!)
    (.selectIndices  (int (first indices)) (int-array (rest indices)))))

(extend-type TreeView
  CollectionView
  (selection [this] (some->> this
                      (.getSelectionModel)
                      (.getSelectedItems)
                      (filter (comp not nil?))
                      (mapv #(.getValue ^TreeItem %))))
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

(defn selection-root-items [^TreeView tree-view path-fn id-fn]
  (let [selection (-> tree-view (.getSelectionModel) (.getSelectedItems))]
    (let [items (into {} (map #(do [(path-fn %) %]) (filter id-fn selection)))
          roots (loop [paths (keys items)
                       roots []]
                  (if-let [path (first paths)]
                    (let [ancestors (filter #(util/seq-starts-with? path %) roots)
                          roots (if (empty? ancestors)
                                  (conj roots path)
                                  roots)]
                      (recur (rest paths) roots))
                    roots))]
      (vals (into {} (map #(let [item (items %)]
                            [(id-fn item) item]) roots))))))

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

;; context handling

;; context for TextInputControls so that we can retain default behaviours

(defn make-text-input-control-context
  [control]
  (handler/->context :text-input-control {:control control}))

(defn- has-selection?
  [^TextInputControl control]
  (not (string/blank? (.getSelectedText control))))

(handler/defhandler :copy :text-input-control
  (enabled? [^TextInputControl control] (has-selection? control))
  (run [^TextInputControl control] (.copy control)))

(handler/defhandler :cut :text-input-control
  (enabled? [^TextInputControl control] (has-selection? control))
  (run [^TextInputControl control] (.cut control)))

(handler/defhandler :paste :text-input-control
  (enabled? [] (.. Clipboard getSystemClipboard hasString))
  (run [^TextInputControl control] (.paste control)))

(handler/defhandler :undo :text-input-control
  (enabled? [^TextInputControl control] (.isUndoable control))
  (run [^TextInputControl control] (.undo control)))

(handler/defhandler :redo :text-input-control
  (enabled? [^TextInputControl control] (.isRedoable control))
  (run [^TextInputControl control] (.redo control)))

(defn ->selection-provider [view]
  (reify handler/SelectionProvider
    (selection [this] (selection view))
    (succeeding-selection [this] [])
    (alt-selection [this] [])))

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
  [^Node initial-node all-selections?]
  (loop [^Node node initial-node
         ctxs []]
    (if-not node
      (handler/eval-contexts ctxs all-selections?)
      (if-let [ctx (context node)]
        (recur (.getParent node) (conj ctxs ctx))
        (recur (.getParent node) ctxs)))))

(defn contexts
  ([^Scene scene]
    (contexts scene true))
  ([^Scene scene all-selections?]
    (node-contexts (or (.getFocusOwner scene) (.getRoot scene)) all-selections?)))

(defn extend-menu [id location menu]
  (menu/extend-menu id location menu))

(defn- make-desc [control menu-id]
  {:control control
   :menu-id menu-id})

(defn- wrap-menu-image [node]
  (doto (Pane.)
    (children! [node])
    (add-style! "menu-image-wrapper")))

(defn- make-submenu [label icon menu-items on-open]
  (when (seq menu-items)
    (let [menu (Menu. label)]
      (when on-open
        (.setOnShowing menu (event-handler e (on-open))))
      (when icon
        (.setGraphic menu (wrap-menu-image (jfx/get-image-view icon 16))))
      (.addAll (.getItems menu) (to-array menu-items))
      menu)))

(defn- make-menu-command [label icon acc user-data command enabled? check action-fn]
  (let [^MenuItem menu-item (if check
                              (CheckMenuItem. label)
                              (MenuItem. label))
        key-combo           (and acc (KeyCombination/keyCombination acc))]
    (when command
      (.setId menu-item (name command)))
    (when key-combo
      (.setAccelerator menu-item key-combo)
      (swap! *menu-key-combos* conj key-combo))
    (when icon
      (.setGraphic menu-item (wrap-menu-image (jfx/get-image-view icon 16))))
    (.setDisable menu-item (not enabled?))
    (.setOnAction menu-item (event-handler event (action-fn)))
    (user-data! menu-item ::menu-user-data user-data)
    menu-item))

(declare make-menu-items)

(defn select-items [items options command-contexts]
  (some-> (handler/active :select-items command-contexts {:items items
                                                          :options options})
    handler/run))

(defn- make-menu-item [^Scene scene item command-contexts]
  (let [icon (:icon item)
        item-label (:label item)
        on-open (:on-submenu-open item)]
    (if-let [children (:children item)]
      (make-submenu item-label icon (make-menu-items scene children command-contexts) on-open)
      (if (= item-label :separator)
        (SeparatorMenuItem.)
        (let [command (:command item)
              user-data (:user-data item)
              check (:check item)]
          (when-let [handler-ctx (handler/active command command-contexts user-data)]
            (let [label (or (handler/label handler-ctx) item-label)
                  enabled? (handler/enabled? handler-ctx)]
              (if-let [options (handler/options handler-ctx)]
                (if (contains? item :acc)
                  (make-menu-command label icon (:acc item) user-data command enabled? check
                                     (fn []
                                       (let [command-contexts (contexts scene)]
                                         (when-let [user-data (some-> (select-items options {:title label
                                                                                             :cell-fn (fn [item]
                                                                                                        {:text (:label item)
                                                                                                         :icon (:icon item)})}
                                                                                   command-contexts)
                                                                first
                                                                :user-data)]
                                           (when-let [handler-ctx (handler/active command command-contexts user-data)]
                                             (when (handler/enabled? handler-ctx)
                                               (handler/run handler-ctx)))))))
                  (make-submenu label icon (make-menu-items scene options command-contexts) on-open))
                (make-menu-command label icon (:acc item) user-data command enabled? check
                                   (fn []
                                     (when-let [handler-ctx (handler/active command (contexts scene) user-data)]
                                       (when (handler/enabled? handler-ctx)
                                         (handler/run handler-ctx)))))))))))))

(defn- make-menu-items [^Scene scene menu command-contexts]
  (let [menu-items (->> menu
                        (map (fn [item] (make-menu-item scene item command-contexts)))
                        (remove nil?))]
    (when-let [head (first menu-items)]
      (add-style! head "first-menu-item"))
    (when-let [tail (last menu-items)]
      (add-style! tail "last-menu-item"))
    menu-items))

(defn- ^ContextMenu make-context-menu [menu-items]
  (let [^ContextMenu context-menu (ContextMenu.)]
    (.addAll (.getItems context-menu) (to-array menu-items))
    context-menu))

(defn register-context-menu [^Control control menu-id]
  (.addEventHandler control ContextMenuEvent/CONTEXT_MENU_REQUESTED
    (event-handler event
                   (when-not (.isConsumed event)
                     (let [^Scene scene (.getScene control)
                           cm (make-context-menu (make-menu-items scene (menu/realize-menu menu-id) (contexts scene false)))]
                       ;; Required for autohide to work when the event originates from the anchor/source control
                       ;; See RT-15160 and Control.java
                       (.setImpl_showRelativeToWindow cm true)
                       (.show cm control (.getScreenX ^ContextMenuEvent event) (.getScreenY ^ContextMenuEvent event))
                       (.consume event))))))

(defn register-tab-context-menu [^Tab tab menu-id]
  (let [^Scene scene (.getScene (.getTabPane tab))
        cm (make-context-menu (make-menu-items scene (menu/realize-menu menu-id) (contexts scene)))]
    (.setImpl_showRelativeToWindow cm true)
    (.setContextMenu tab cm)))

(defn register-menubar [^Scene scene  menubar-id menu-id ]
  ; TODO: See comment below about top-level items. Should be enforced here
 (let [root (.getRoot scene)]
   (if-let [menubar (.lookup root menubar-id)]
     (let [desc (make-desc menubar menu-id)]
       (user-data! root ::menubar desc))
     (log/warn :message (format "menubar %s not found" menubar-id))))
  (.addEventFilter scene KeyEvent/KEY_PRESSED
    (event-handler event
      (when (some (fn [^KeyCombination c] (.match c event)) @*menu-key-combos*)
        (.consume event)))))

(defn run-command
  ([^Node node command]
    (run-command node command {}))
  ([^Node node command user-data]
    (run-command node command user-data true nil))
  ([^Node node command user-data all-selections? success-fn]
    (let [user-data (or user-data {})
          command-contexts (node-contexts node all-selections?)]
      (when-let [handler-ctx (handler/active command command-contexts user-data)]
        (when (handler/enabled? handler-ctx)
          (let [result (handler/run handler-ctx)]
            (when success-fn
              (success-fn))
            result))))))

(defn bind-action!
  ([^Node node command]
    (bind-action! node command {}))
  ([^Node node command user-data]
    (user-data! node ::bound-action {:command command :user-data user-data})
    (on-action! node (fn [^Event e] (run-command node command user-data true (fn [] (.consume e)))))))

(defn refresh-bound-action-enabled!
  [^Node node]
  (let [{:keys [command user-data]
         :or {:user-data {}}} (user-data node ::bound-action)
        command-contexts (node-contexts node true)
        handler-ctx (handler/active command command-contexts user-data)
        enabled (and handler-ctx
                     (handler/enabled? handler-ctx))]
    (disable! node (not enabled))))

(defn bind-double-click!
  ([^Node node command]
    (bind-double-click! node command {}))
  ([^Node node command user-data]
    (.addEventFilter node MouseEvent/MOUSE_CLICKED
      (event-handler e (when (= 2 (.getClickCount ^MouseEvent e))
                         (run-command node command user-data false (fn [] (.consume e))))))))

(defn bind-keys! [^Node node key-bindings]
  (.addEventFilter node KeyEvent/KEY_PRESSED
    (event-handler event
                   (let [code (.getCode ^KeyEvent event)]
                     (when-let [binding (get key-bindings code)]
                       (let [[command user-data] (if (vector? binding)
                                                   binding
                                                   [binding {}])]
                         (run-command node command user-data true (fn [] (.consume event)))))))))

(def ^:private invalidate-menus? (atom false))

(defn invalidate-menus! []
  (reset! invalidate-menus? true))

(defn- refresh-menubar [md command-contexts]
 (let [menu (menu/realize-menu (:menu-id md))
       control ^MenuBar (:control md)]
   (when (or
          @invalidate-menus?
          (not= menu (user-data control ::menu))
          (not= command-contexts (user-data control ::command-contexts)))

     (reset! invalidate-menus? false)
     (.clear (.getMenus control))
     ; TODO: We must ensure that top-level element are of type Menu and note MenuItem here, i.e. top-level items with ":children"
     (.addAll (.getMenus control) (to-array (make-menu-items (.getScene control) menu command-contexts)))
     (user-data! control ::menu menu)
     (user-data! control ::command-contexts command-contexts))))

(defn- refresh-menu-state [^Menu menu command-contexts]
  (when (not (.isShowing menu))
    (doseq [m (.getItems menu)]
      (cond
        (instance? Menu m)
        (refresh-menu-state m command-contexts)

        (instance? CheckMenuItem m)
        (let [m         ^CheckMenuItem m
              command   (keyword (.getId ^MenuItem m))
              user-data (user-data m ::menu-user-data)
              handler-ctx (handler/active command command-contexts user-data)]
          (doto m
            (.setDisable (not (handler/enabled? handler-ctx)))
            (.setSelected (boolean (handler/state handler-ctx)))))

        (instance? MenuItem m)
        (let [m ^MenuItem m]
          (.setDisable m (not (-> (handler/active (keyword (.getId m))
                                                  command-contexts
                                                  (user-data m ::menu-user-data))
                                handler/enabled?))))))))

(defn- refresh-menubar-state [^MenuBar menubar command-contexts]
  (doseq [m (.getMenus menubar)]
    (refresh-menu-state m command-contexts)))

(defn register-toolbar [^Scene scene toolbar-id menu-id ]
  (let [root (.getRoot scene)]
    (if-let [toolbar (.lookup root toolbar-id)]
      (let [desc (make-desc toolbar menu-id)]
        (user-data! root ::toolbars (assoc-in (user-data root ::toolbars) [toolbar-id] desc)))
      (log/warn :message (format "toolbar %s not found" toolbar-id)))))

(defn- refresh-toolbar [td command-contexts]
 (let [menu (menu/realize-menu (:menu-id td))
       ^Pane control (:control td)]
   (when-not (and (= menu (user-data control ::menu))
                  (= command-contexts (user-data control ::command-contexts)))
     (.clear (.getChildren control))
     (user-data! control ::menu menu)
     (user-data! control ::command-contexts command-contexts)
     (let [children (doall
                      (for [menu-item menu
                            :let [command (:command menu-item)
                                  user-data (:user-data menu-item)
                                  separator? (= :separator (:label menu-item))
                                  handler-ctx (handler/active command command-contexts user-data)]
                            :when (or separator? handler-ctx)]
                        (let [^Control child (if separator?
                                               (doto (Separator. Orientation/VERTICAL)
                                                 (add-style! "separator"))
                                               (if-let [opts (handler/options handler-ctx)]
                                                 (let [hbox (doto (HBox.)
                                                              (add-style! "cell"))
                                                       cb (doto (ChoiceBox.)
                                                            (.setConverter (proxy [StringConverter] []
                                                                             (fromString [str] (some #{str} (map :label opts)))
                                                                             (toString [v] (:label v)))))]
                                                   (observe (.valueProperty cb) (fn [this old new]
                                                                                  (when new
                                                                                    (let [command-contexts (contexts (.getScene control))]
                                                                                      (when-let [handler-ctx (handler/active (:command new) command-contexts (:user-data new))]
                                                                                        (when (handler/enabled? handler-ctx)
                                                                                          (handler/run handler-ctx)))))))
                                                   (doseq [opt opts]
                                                     (.add (.getItems cb) opt))
                                                   (.add (.getChildren hbox) (jfx/get-image-view (:icon menu-item) 22.5))
                                                   (.add (.getChildren hbox) cb)
                                                   hbox)
                                                 (let [button (ToggleButton. (or (handler/label handler-ctx) (:label menu-item)))
                                                       icon (:icon menu-item)
                                                       selection-provider (:selection-provider td)]
                                                   (when icon
                                                     (.setGraphic button (jfx/get-image-view icon 22.5)))
                                                   (when command
                                                     (on-action! button (fn [event]
                                                                          (let [command-contexts (contexts (.getScene control))]
                                                                            (when-let [handler-ctx (handler/active command command-contexts user-data)]
                                                                              (when (handler/enabled? handler-ctx)
                                                                                (handler/run handler-ctx)))))))
                                                   button)))]
                          (when command
                            (.setId child (name command)))
                          (user-data! child ::menu-user-data user-data)
                          child)))
           children (if (instance? Separator (last children))
                      (butlast children)
                      children)]
       (doseq [child children]
         (.add (.getChildren control) child))))))

(defn- refresh-toolbar-state [^Pane toolbar command-contexts]
  (let [nodes (.getChildren toolbar)
        ids (map #(.getId ^Node %) nodes)]
    (doseq [^Node n nodes
            :let [user-data (user-data n ::menu-user-data)
                  handler-ctx (handler/active (keyword (.getId n)) command-contexts user-data)]]
      (disable! n (not (handler/enabled? handler-ctx)))
      (when (instance? ToggleButton n)
        (if (handler/state handler-ctx)
          (.setSelected ^Toggle n true)
          (.setSelected ^Toggle n false)))
      (when (instance? HBox n)
        (let [^HBox box n
              state (handler/state handler-ctx)
              ^ChoiceBox cb (.get (.getChildren box) 1)]
          (when (not (.isShowing cb))
            (let [items (.getItems cb)
                  opts (vec items)
                  new-opts (vec (handler/options handler-ctx))]
              (when (not= opts new-opts)
                (.setAll items ^java.util.Collection new-opts)))
            (let [selection-model (.getSelectionModel cb)
                  item (.getSelectedItem selection-model)]
              (when (not= item state)
                (.select selection-model state)))))))))

(defn refresh
  ([^Scene scene] (refresh scene (contexts scene)))
  ([^Scene scene command-contexts]
   (let [root (.getRoot scene)
         toolbar-descs (vals (user-data root ::toolbars))]
     (when-let [md (user-data root ::menubar)]
       (refresh-menubar md command-contexts)
       (refresh-menubar-state (:control md) command-contexts))
     (doseq [td toolbar-descs]
       (refresh-toolbar td command-contexts)
       (refresh-toolbar-state (:control td) command-contexts)))))

(defn update-progress-controls! [progress ^ProgressBar bar ^Label label]
  (let [pctg (progress/percentage progress)]
    (.setProgress bar (if (nil? pctg) -1.0 (double pctg)))
    (when label
      (.setText label (progress/description progress)))))

(defn- update-progress!
  [progress]
  (let [root  (.. (main-stage) (getScene) (getRoot))
         tb    (.lookup root "#toolbar-status")
         bar   (.lookup tb ".progress-bar")
         label (.lookup tb ".label")]
    (update-progress-controls! progress bar label)))

(defn default-render-progress! [progress]
  (run-later (update-progress! progress)))

(defn init-progress!
  []
  (update-progress! progress/done))

(defmacro with-progress [bindings & body]
  `(let ~bindings
     (try
       ~@body
       (finally
         ((second ~bindings) progress/done)))))

(defn modal-progress [title total-work worker-fn]
  (run-now
   (let [root             ^Parent (load-fxml "progress.fxml")
         stage            (make-stage)
         scene            (Scene. root)
         title-control    ^Label (.lookup root "#title")
         progress-control ^ProgressBar (.lookup root "#progress")
         message-control  ^Label (.lookup root "#message")
         return           (atom nil)
         render-progress! (fn [progress]
                            (run-later
                             (update-progress-controls! progress progress-control message-control)))]
      (.setText title-control title)
      (.setProgress progress-control 0)
      (.initOwner stage @*main-stage*)
      (.initModality stage Modality/WINDOW_MODAL)
      (.setScene stage scene)
      (future
        (try
          (reset! return (worker-fn render-progress!))
          (catch Throwable e
            (log/error :exception e)
            (reset! return e)))
        (run-later (.close stage)))
      (.showAndWait stage)
      (if (instance? Throwable @return)
          (throw @return)
          @return))))

(def ^:private last-focused-node (atom nil))

(defn disable-ui [disabled]
  (let [scene       (.getScene (main-stage))
        focus-owner (.getFocusOwner scene)
        root        (.getRoot scene)]
    (.setDisable root disabled)
    (when-let [^Node node @last-focused-node]
      (.requestFocus node))
    (reset! last-focused-node focus-owner)))

(defmacro with-disabled-ui [& body]
  `(try
     (run-now (disable-ui true))
     ~@body
     (finally
       (run-now (disable-ui false)))))

(defn mouse-type
  []
  :one-button)

(defn- on-ui-thread?
  []
  (Platform/isFxApplicationThread))

(defmacro on-app-thread
  [& body]
  `(if (on-ui-thread?)
     (do ~@body)
     (Platform/runLater
      (bound-fn [] (do ~@body)))))

(defn run-wait
  [f]
  (let [result (promise)]
    (on-app-thread
     (deliver result (f)))
    @result))

(defmacro run-safe
  [& body]
  `(Platform/runLater
    (fn [] ~@body)))

(defn handle
  [f]
  (reify EventHandler
    (handle [this event] (f event))))

(defn weak [^EventHandler h]
  (WeakEventHandler. h))

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
    #_(swt-await
      (doseq [l (vals @listeners)]
       (run-safe
         (l event))))))

(defn make-event-broadcaster [] (EventBroadcaster. (atom {})))

(defmacro defcommand
  "Create a command with the given category and id. Binds
the resulting command to the named variable.

Label should be a human-readable string. It will appear
directly in the UI (unless there is a translation for it.)

If you use the same category-id and command-id more than once,
this will create independent entities that refer to the same underlying
command."
  [name category-id command-id label]
  `(def ^:command ~name [~label ~category-id ~command-id]))

(defprotocol Future
  (cancel [this])
  (restart [this]))

(extend-type Timeline
  Future
  (cancel [this] (.stop this))
  (restart [this] (.playFromStart this)))

(defn ->future [delay run-fn]
  (let [^EventHandler handler (event-handler e (run-fn))
        ^"[Ljavafx.animation.KeyValue;" values (into-array KeyValue [])]
    ; TODO - fix reflection ctor warning
    (doto (Timeline. 60 (into-array KeyFrame [(KeyFrame. ^Duration (Duration/seconds delay) handler values)]))
      (.play))))

(defn ->timer
  ([name tick-fn]
    (->timer nil name tick-fn))
  ([fps name tick-fn]
   (let [last       (atom (System/nanoTime))
         interval   (when fps
                      (long (* 1e9 (/ 1 (double fps)))))]
     {:last  last
      :timer (proxy [AnimationTimer] []
               (handle [now]
                 (profiler/profile "timer" name
                                   (let [delta (- now @last)]
                                     (when (or (nil? interval) (> delta interval))
                                       (try
                                         (tick-fn this (* delta 1e-9))
                                         (reset! last (- now (if interval
                                                               (- delta interval)
                                                               0)))
                                         (catch Throwable t
                                           (error-reporting/report-exception! t))))))))})))

(defn timer-start! [timer]
  (.start ^AnimationTimer (:timer timer)))

(defn timer-stop! [timer]
  (.stop ^AnimationTimer (:timer timer)))

(defn anim! [duration anim-fn end-fn]
  (let [duration   (long (* 1e9 duration))
        start      (System/nanoTime)
        end        (+ start (long duration))]
    (doto (proxy [AnimationTimer] []
            (handle [now]
              (if (< now end)
                (let [t (/ (double (- now start)) duration)]
                  (try
                    (anim-fn t)
                    (catch Throwable t
                      (error-reporting/report-exception! t))))
                (try
                  (end-fn)
                  (catch Throwable t
                    (error-reporting/report-exception! t))
                  (finally
                    (.stop ^AnimationTimer this))))))
      (.start))))

(defn anim-stop! [^AnimationTimer anim]
  (.stop anim))

(defn- chain-handler [new-handler-fn ^EventHandler existing-handler]
  (event-handler e
                 (new-handler-fn e)
                 (when existing-handler
                   (.handle existing-handler e))))

(defprotocol CloseRequestable
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

(defprotocol Closeable
  (on-closed [this])
  (on-closed! [this f]))

(extend-protocol Closeable
  javafx.scene.control.Tab
  (on-closed [this] (.getOnClosed this))
  (on-closed! [this f] (.setOnClosed this (chain-handler f (on-closed this))))

  javafx.stage.Stage
  (on-closed [this] (.getOnHidden this))
  (on-closed! [this f] (.setOnHidden this (chain-handler f (on-closed this)))))

(defn timer-stop-on-closed!
  [closeable timer]
  (on-closed! closeable (fn [_]
                         (timer-stop! timer))))

(defn drag-internal? [^DragEvent e]
  (some? (.getGestureSource e)))

(defn parent->stage ^Stage [^Parent parent]
  (.. parent getScene getWindow))

(defn browse-url
  [url]
  (.browse (Desktop/getDesktop) (URI. url)))

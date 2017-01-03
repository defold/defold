(ns editor.app-view
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.changes-view :as changes-view]
            [editor.dialogs :as dialogs]
            [editor.engine :as engine]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.login :as login]
            [editor.defold-project :as project]
            [editor.defold-project-search :as project-search]
            [editor.github :as github]
            [editor.prefs :as prefs]
            [editor.prefs-dialog :as prefs-dialog]
            [editor.progress :as progress]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.graph-util :as gu]
            [editor.util :as util]
            [editor.targets :as targets]
            [editor.build-errors-view :as build-errors-view]
            [editor.hot-reload :as hot-reload]
            [editor.view :as view]
            [util.profiler :as profiler]
            [util.http-server :as http-server])
  (:import [com.defold.editor EditorApplication]
           [com.defold.editor Start]
           [java.awt Desktop]
           [java.util Collection]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList ListChangeListener]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent Event EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem Menu MenuItem MenuBar TabPane Tab ProgressBar Tooltip SplitPane]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser WindowEvent]
           [javafx.util Callback]
           [java.io File ByteArrayOutputStream]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [com.jogamp.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

(set! *warn-on-reflection* true)

(g/defnode AppView
  (property stage Stage)
  (property tab-pane TabPane)
  (property auto-pulls g/Any)
  (property active-tool g/Keyword)

  (input open-views g/Any :array)
  (input outline g/Any)
  (input project-id g/NodeID)
  (input selected-node-ids-by-resource-node g/Any)
  (input selected-node-properties-by-resource-node g/Any)
  (input sub-selections-by-resource-node g/Any)

  (output open-views g/Any :cached (g/fnk [open-views] (into {} open-views)))
  (output active-tab Tab (g/fnk [^TabPane tab-pane] (some-> tab-pane (.getSelectionModel) (.getSelectedItem))))
  (output active-outline g/Any (gu/passthrough outline))
  (output active-view g/NodeID (g/fnk [^Tab active-tab]
                                   (when active-tab
                                     (ui/user-data active-tab ::view))))
  (output active-resource-node g/NodeID :cached (g/fnk [active-view open-views] (:resource-node (get open-views active-view))))
  (output active-resource resource/Resource :cached (g/fnk [active-view open-views] (:resource (get open-views active-view))))
  (output open-resource-nodes g/Any :cached (g/fnk [open-views] (->> open-views vals (map :resource-node))))
  (output selected-node-ids g/Any (g/fnk [selected-node-ids-by-resource-node active-resource-node]
                                    (get selected-node-ids-by-resource-node active-resource-node)))
  (output selected-node-properties g/Any (g/fnk [selected-node-properties-by-resource-node active-resource-node]
                                           (get selected-node-properties-by-resource-node active-resource-node)))
  (output sub-selection g/Any (g/fnk [sub-selections-by-resource-node active-resource-node]
                                (get sub-selections-by-resource-node active-resource-node)))
  (output refresh-tab-pane g/Any :cached (g/fnk [^TabPane tab-pane open-views]
                                           (let [open-tabs (filter (fn [^Tab tab] (get open-views (ui/user-data tab ::view))) (.getTabs tab-pane))]
                                             (doseq [^Tab tab open-tabs
                                                     :let [resource (:resource (get open-views (ui/user-data tab ::view)))]]
                                               (ui/text! tab (resource/resource-name resource)))
                                             (.setAll (.getTabs tab-pane) ^Collection open-tabs)))))

(defn- disconnect-sources [target-node target-label]
  (for [[source-node source-label] (g/sources-of target-node target-label)]
    (g/disconnect source-node source-label target-node target-label)))

(defn- replace-connection [source-node source-label target-node target-label]
  (concat
    (disconnect-sources target-node target-label)
    (if (and source-node (contains? (-> source-node g/node-type* g/output-labels) source-label))
      (g/connect source-node source-label target-node target-label)
      [])))

(defn- on-selected-tab-changed [app-view resource-node]
  (g/transact
    (replace-connection resource-node :node-outline app-view :outline))
  (g/invalidate! [[app-view :active-tab]]))

(handler/defhandler :move-tool :workbench
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :move)))
  (state [app-view] (= (g/node-value app-view :active-tool) :move)))

(handler/defhandler :scale-tool :workbench
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :scale)))
  (state [app-view]  (= (g/node-value app-view :active-tool) :scale)))

(handler/defhandler :rotate-tool :workbench
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :rotate)))
  (state [app-view]  (= (g/node-value app-view :active-tool) :rotate)))

(ui/extend-menu :toolbar nil
                [{:id :select
                  :icon "icons/45/Icons_T_01_Select.png"
                  :command :select-tool}
                 {:id :move
                  :icon "icons/45/Icons_T_02_Move.png"
                  :command :move-tool}
                 {:id :rotate
                  :icon "icons/45/Icons_T_03_Rotate.png"
                  :command :rotate-tool}
                 {:id :scale
                  :icon "icons/45/Icons_T_04_Scale.png"
                  :command :scale-tool}])

(def ^:const prefs-window-dimensions "window-dimensions")
(def ^:const prefs-split-positions "split-positions")

(handler/defhandler :quit :global
  (enabled? [] true)
  (run []
    (let [^Stage main-stage (ui/main-stage)]
      (.fireEvent main-stage (WindowEvent. main-stage WindowEvent/WINDOW_CLOSE_REQUEST)))))

(handler/defhandler :target :global
  (run [user-data prefs]
    (when user-data
      (prefs/set-prefs prefs "last-target" user-data)))
  (state [user-data prefs]
         (let [last-target (prefs/get-prefs prefs "last-target" nil)]
           (= user-data last-target)))
  (options [user-data prefs]
           (when-not user-data
             (let [targets     (targets/get-targets)
                   last-target (when-let [lt (prefs/get-prefs prefs "last-target" nil)]
                                 [lt])]
               (mapv (fn [target]
                       (let [[_ _ ip] (re-matches #"^(http://)([\w\.]+)(:)(.*)$" (:url target))]
                         {:label     (format "%s (%s)" (:name target) ip)
                          :command   :target
                          :check     true
                          :user-data target}))
                     (distinct (concat last-target targets)))))))

(handler/defhandler :target-ip :global
  (run [prefs]
    (ui/run-later
     (when-let [ip (dialogs/make-target-ip-dialog)]
       (let [url (format "http://%s:8001" ip)]
         (prefs/set-prefs prefs "last-target" {:name "Manual IP"
                                               :url  url})
         (ui/invalidate-menus!))))))

(handler/defhandler :target-log :global
  (run []
    (ui/run-later (targets/make-target-log-dialog))))

(defn store-window-dimensions [^Stage stage prefs]
  (let [dims    {:x      (.getX stage)
                 :y      (.getY stage)
                 :width  (.getWidth stage)
                 :height (.getHeight stage)}]
    (prefs/set-prefs prefs prefs-window-dimensions dims)))

(defn restore-window-dimensions [^Stage stage prefs]
  (when-let [dims (prefs/get-prefs prefs prefs-window-dimensions nil)]
    (.setX stage (:x dims))
    (.setY stage (:y dims))
    (.setWidth stage (:width dims))
    (.setHeight stage (:height dims))))

(def ^:private legacy-split-ids ["main-split"
                                 "center-split"
                                 "right-split"
                                 "assets-split"])

(def ^:private split-ids ["main-split"
                          "workbench-split"
                          "center-split"
                          "right-split"
                          "assets-split"])

(defn store-split-positions! [^Stage stage prefs]
  (let [^Node root (.getRoot (.getScene stage))
        controls (ui/collect-controls root split-ids)
        div-pos (into {} (map (fn [[id ^SplitPane sp]] [id (.getDividerPositions sp)]) controls))]
    (prefs/set-prefs prefs prefs-split-positions div-pos)))

(defn restore-split-positions! [^Stage stage prefs]
  (when-let [div-pos (prefs/get-prefs prefs prefs-split-positions nil)]
    (let [^Node root (.getRoot (.getScene stage))
          controls (ui/collect-controls root split-ids)
          div-pos (if (vector? div-pos)
                    (zipmap (map keyword legacy-split-ids) div-pos)
                    div-pos)]
      (doseq [[k pos] div-pos
              :let [^SplitPane sp (get controls k)]
              :when sp]
        (.setDividerPositions sp (into-array Double/TYPE pos))))))

(handler/defhandler :open-project :global
  (run [] (when-let [file-name (ui/choose-file "Open Project" "Project Files" ["*.project"])]
            (EditorApplication/openEditor (into-array String [file-name])))))

(handler/defhandler :logout :global
  (run [prefs] (login/logout prefs)))

(handler/defhandler :preferences :global
  (run [prefs] (prefs-dialog/open-prefs prefs)))

(defn- remove-tab [^TabPane tab-pane ^Tab tab]
  (.remove (.getTabs tab-pane) tab)
  ;; TODO: Workaround as there's currently no API to close tabs programatically with identical semantics to close manually
  ;; See http://stackoverflow.com/questions/17047000/javafx-closing-a-tab-in-tabpane-dynamically
  (Event/fireEvent tab (Event. Tab/CLOSED_EVENT)))

(defn- collect-resources [{:keys [children] :as resource}]
  (if (empty? children)
    #{resource}
    (set (concat [resource] (mapcat collect-resources children)))))

(defn- get-tabs [app-view]
  (let [tab-pane ^TabPane (g/node-value app-view :tab-pane)]
    (.getTabs tab-pane)))

(defn- build-project [project build-errors-view]
  (let [build-options {:clear-errors! (fn [] (build-errors-view/clear-build-errors build-errors-view))
                       :render-error! (fn [errors]
                                        (ui/run-later
                                          (build-errors-view/update-build-errors
                                            build-errors-view
                                            errors)))}]
    (project/build-and-save-project project build-options)))

(handler/defhandler :build :global
  (enabled? [] (not (project/ongoing-build-save?)))
  (run [workspace project prefs web-server build-errors-view]
    (let [build (build-project project build-errors-view)]
      (when (and (future? build) @build)
        (or (when-let [target (prefs/get-prefs prefs "last-target" targets/local-target)]
              (let [local-url (format "http://%s:%s%s" (:local-address target) (http-server/port web-server) hot-reload/url-prefix)]
                (engine/reboot (:url target) local-url)))
            (engine/launch prefs workspace (io/file (workspace/project-path (g/node-value project :workspace)))))))))

(handler/defhandler :hot-reload :global
  (enabled? [app-view]
            (g/node-value app-view :active-resource))
  (run [project app-view prefs build-errors-view]
    (when-let [resource (g/node-value app-view :active-resource)]
      (let [build (build-project project build-errors-view)]
        (when (and (future? build) @build)
            (engine/reload-resource (:url (prefs/get-prefs prefs "last-target" targets/local-target)) resource))))))

(handler/defhandler :close :global
  (enabled? [app-view] (not-empty (get-tabs app-view)))
  (run [app-view]
    (let [tab-pane ^TabPane (g/node-value app-view :tab-pane)]
      (when-let [tab (-> tab-pane (.getSelectionModel) (.getSelectedItem))]
        (remove-tab tab-pane tab)))))

(handler/defhandler :close-other :global
  (enabled? [app-view] (not-empty (get-tabs app-view)))
  (run [app-view]
    (let [tab-pane ^TabPane (g/node-value app-view :tab-pane)]
      (when-let [selected-tab (-> tab-pane (.getSelectionModel) (.getSelectedItem))]
        (doseq [tab (.getTabs tab-pane)]
          (when (not= tab selected-tab)
            (remove-tab tab-pane tab)))))))

(handler/defhandler :close-all :global
  (enabled? [app-view] (not-empty (get-tabs app-view)))
  (run [app-view]
    (let [tab-pane ^TabPane (g/node-value app-view :tab-pane)]
      (doseq [tab (.getTabs tab-pane)]
        (remove-tab tab-pane tab)))))

(defn make-about-dialog []
  (let [root ^Parent (ui/load-fxml "about.fxml")
        stage (ui/make-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["version" "sha1" "time"])]
    (ui/text! (:version controls) (str "Version: " (System/getProperty "defold.version" "NO VERSION")))
    (ui/text! (:sha1 controls) (System/getProperty "defold.sha1" "NO SHA1"))
    (ui/text! (:time controls) (System/getProperty "defold.buildtime" "NO BUILD TIME"))
    (ui/title! stage "About")
    (.setScene stage scene)
    (ui/show! stage)))

(handler/defhandler :documentation :global
  (run [] (ui/browse-url "http://www.defold.com/learn/")))

(handler/defhandler :report-issue :global
  (run [] (ui/browse-url (github/new-issue-link))))

(handler/defhandler :report-praise :global
  (run [] (ui/browse-url (github/new-praise-link))))

(handler/defhandler :about :global
  (run [] (make-about-dialog)))

(handler/defhandler :reload-stylesheet :global
  (run [] (ui/reload-root-styles!)))

(ui/extend-menu ::menubar nil
                [{:label "File"
                  :id ::file
                  :children [{:label "New..."
                              :id ::new
                              :acc "Shortcut+N"
                              :command :new-file}
                             {:label "Open..."
                              :id ::open
                              :acc "Shortcut+O"
                              :command :open}
                             {:label "Save All"
                              :id ::save-all
                              :acc "Shortcut+S"
                              :command :save-all}
                             {:label :separator}
                             {:label "Open Asset"
                              :acc "Shift+Shortcut+R"
                              :command :open-asset}
                             {:label "Search in Files"
                              :acc "Shift+Shortcut+F"
                              :command :search-in-files}
                             {:label :separator}
                             {:label "Hot Reload"
                              :acc "Shortcut+R"
                              :command :hot-reload}
                             {:label "Close"
                              :acc "Shortcut+W"
                              :command :close}
                             {:label "Close All"
                              :acc "Shift+Shortcut+W"
                              :command :close-all}
                             {:label "Close Others"
                              :command :close-other}
                             {:label :separator}
                             {:label "Logout"
                              :command :logout}
                             {:label "Preferences..."
                              :command :preferences
                              :acc "Shortcut+,"}
                             {:label "Quit"
                              :acc "Shortcut+Q"
                              :command :quit}]}
                 {:label "Edit"
                  :id ::edit
                  :children [{:label "Undo"
                              :acc "Shortcut+Z"
                              :icon "icons/undo.png"
                              :command :undo}
                             {:label "Redo"
                              :acc "Shift+Shortcut+Z"
                              :icon "icons/redo.png"
                              :command :redo}
                             {:label :separator}
                             {:label "Copy"
                              :acc "Shortcut+C"
                              :command :copy}
                             {:label "Cut"
                              :acc "Shortcut+X"
                              :command :cut}
                             {:label "Paste"
                              :acc "Shortcut+V"
                              :command :paste}
                             {:label "Delete"
                              :acc "Shortcut+BACKSPACE"
                              :icon_ "icons/redo.png"
                              :command :delete}
                             {:label :separator}
                             {:label "Move Up"
                              :acc "Alt+UP"
                              :command :move-up}
                             {:label "Move Down"
                              :acc "Alt+DOWN"
                              :command :move-down}
                             {:label :separator
                              :id ::edit-end}]}
                 {:label "Help"
                  :children [{:label "Profiler"
                              :children [{:label "Measure"
                                          :command :profile
                                          :acc "Shortcut+Alt+X"}
                                         {:label "Measure and Show"
                                          :command :profile-show
                                          :acc "Shift+Shortcut+Alt+X"}]}
                             {:label "Reload Stylesheet"
                              :acc "F5"
                              :command :reload-stylesheet}
                             {:label "Documentation"
                              :command :documentation}
                             {:label "Report Issue"
                              :command :report-issue}
                             {:label "Report Praise"
                              :command :report-praise}
                             {:label "About"
                              :command :about}]}])

(ui/extend-menu ::tab-menu nil
                [{:label "Hot Reload"
                  :acc "Shortcut+R"
                  :command :hot-reload}
                 {:label "Close"
                  :acc "Shortcut+W"
                  :command :close}
                 {:label "Close Others"
                  :command :close-other}
                 {:label "Close All"
                  :acc "Shift+Shortcut+W"
                  :command :close-all}])

(defrecord SelectionProvider [app-view]
  handler/SelectionProvider
  (selection [this] (g/node-value app-view :selected-node-ids))
  (succeeding-selection [this] [])
  (alt-selection [this] []))

(defn ->selection-provider [app-view] (SelectionProvider. app-view))

(defn- update-selection [s open-resource-nodes active-resource-node selection-value]
  (->> (assoc s active-resource-node selection-value)
    (filter (comp (set open-resource-nodes) first))
    (into {})))

(defn select
  ([app-view node-ids]
    (select app-view (g/node-value app-view :active-resource-node) node-ids))
  ([app-view resource-node node-ids]
    (let [project-id (g/node-value app-view :project-id)
          open-resource-nodes (g/node-value app-view :open-resource-nodes)]
      (project/select project-id resource-node node-ids open-resource-nodes))))

(defn select!
  ([app-view node-ids]
    (select! app-view node-ids (gensym)))
  ([app-view node-ids op-seq]
    (g/transact
      (concat
        (g/operation-sequence op-seq)
        (g/operation-label "Select")
        (select app-view node-ids)))))

(defn sub-select!
  ([app-view sub-selection]
    (sub-select! app-view sub-selection (gensym)))
  ([app-view sub-selection op-seq]
    (let [project-id (g/node-value app-view :project-id)
          active-resource-node (g/node-value app-view :active-resource-node)
          open-resource-nodes (g/node-value app-view :open-resource-nodes)]
      (g/transact
        (concat
          (g/operation-sequence op-seq)
          (g/operation-label "Select")
          (project/sub-select project-id active-resource-node sub-selection open-resource-nodes))))))

(defn- make-title
  ([] "Defold Editor 2.0")
  ([project-title] (str (make-title) " - " project-title)))

(defn- refresh-app-title! [^Stage stage project]
  (let [settings      (g/node-value project :settings)
        project-title (settings ["project" "title"])
        new-title     (make-title project-title)]
    (when (not= (.getTitle stage) new-title)
      (.setTitle stage new-title))))

(defn- refresh-ui! [^Stage stage project]
  (when-not (ui/ui-disabled?)
    (ui/refresh (.getScene stage))
    (refresh-app-title! stage project)))

(defn- refresh-views! [app-view]
  (when-not (ui/ui-disabled?)
    (let [auto-pulls (g/node-value app-view :auto-pulls)]
      (doseq [[node label] auto-pulls]
        (profiler/profile "view" (:name @(g/node-type* node))
                          (g/node-value node label))))))

;; Here only because reports indicate that isDesktopSupported does
;; some kind of initialization behind the scenes on Linux:
;; http://stackoverflow.com/questions/23176624/javafx-freeze-on-desktop-openfile-desktop-browseuri
(def desktop-supported? (delay (Desktop/isDesktopSupported)))

(defn- tab->resource-node [^Tab tab]
  (some-> tab
    (ui/user-data ::view)
    (g/node-value :view-data)
    second
    :resource-node))

(defn make-app-view [view-graph workspace project ^Stage stage ^MenuBar menu-bar ^TabPane tab-pane prefs]
  (.setUseSystemMenuBar menu-bar true)
  (.setTitle stage (make-title))
  (force desktop-supported?)
  (let [app-view (first (g/tx-nodes-added (g/transact (g/make-node view-graph AppView :stage stage :tab-pane tab-pane :active-tool :move))))]
    (-> tab-pane
      (.getSelectionModel)
      (.selectedItemProperty)
      (.addListener
        (reify ChangeListener
          (changed [this observable old-val new-val]
            (->> (tab->resource-node new-val)
              (on-selected-tab-changed app-view))))))
    (-> tab-pane
      (.getTabs)
      (.addListener
        (reify ListChangeListener
          (onChanged [this change]
            (ui/restyle-tabs! tab-pane)))))

    (ui/register-toolbar (.getScene stage) "#toolbar" :toolbar)
    (ui/register-menubar (.getScene stage) "#menu-bar" ::menubar)

    (let [refresh-timers [(ui/->timer 3 "refresh-ui" (fn [_ dt] (refresh-ui! stage project)))
                          (ui/->timer 13 "refresh-views" (fn [_ dt] (refresh-views! app-view)))]]
      (doseq [timer refresh-timers]
        (ui/timer-stop-on-closed! stage timer)
        (ui/timer-start! timer)))
    app-view))

(defn- make-tab! [app-view workspace project resource resource-node
                  resource-type view-type make-view-fn ^ObservableList tabs opts]
  (let [parent     (AnchorPane.)
        tab        (doto (Tab. (resource/resource-name resource))
                     (.setContent parent)
                     (ui/user-data! ::view-type view-type))
        view-graph (g/make-graph! :history false :volatility 2)
        opts       (merge opts
                          (get (:view-opts resource-type) (:id view-type))
                          {:app-view  app-view
                           :project   project
                           :workspace workspace
                           :tab       tab})
        view       (make-view-fn view-graph parent resource-node opts)]
    (assert (g/node-instance? view/WorkbenchView view))
    (g/transact
      (concat
        (g/connect resource-node :_node-id view :resource-node)
        (g/connect resource-node :node-id+resource view :node-id+resource)
        (g/connect view :view-data app-view :open-views)))
    (ui/user-data! tab ::view view)
    (.add tabs tab)
    (g/transact
      (select app-view resource-node [resource-node]))
    (.setGraphic tab (jfx/get-image-view (:icon resource-type "icons/64/Icons_29-AT-Unknown.png") 16))
    (ui/register-tab-context-menu tab ::tab-menu)
    (let [close-handler (.getOnClosed tab)]
      (.setOnClosed tab (ui/event-handler
                         event
                         (g/delete-graph! view-graph)
                         (when close-handler
                           (.handle close-handler event)))))
    tab))

(defn open-resource
  ([app-view workspace project resource]
   (open-resource app-view workspace project resource {}))
  ([app-view workspace project resource opts]
   (let [resource-type (resource/resource-type resource)
         view-type     (or (:selected-view-type opts)
                           (first (:view-types resource-type))
                           (workspace/get-view-type workspace :text))]
     (if-let [make-view-fn (:make-view-fn view-type)]
       (let [resource-node     (project/get-resource-node project resource)
             ^TabPane tab-pane (g/node-value app-view :tab-pane)
             tabs              (.getTabs tab-pane)
             tab               (or (some #(when (and (= (tab->resource-node %) resource-node)
                                                  (= view-type (ui/user-data % ::view-type)))
                                            %)
                                     tabs)
                                 (make-tab! app-view workspace project resource resource-node
                                   resource-type view-type make-view-fn tabs opts))]
         (.select (.getSelectionModel tab-pane) tab)
         (when-let [focus (:focus-fn view-type)]
           (focus (ui/user-data tab ::view) opts)))
       (let [^String path (or (resource/abs-path resource)
                            (resource/temp-path resource))]
         (try
           (.open (Desktop/getDesktop) (File. path))
           (catch Exception _
             (dialogs/make-alert-dialog (str "Unable to open external editor for " path)))))))))

(defn- selection->resource-files [selection]
  (when-let [resources (handler/adapt-every selection resource/Resource)]
    (vec (keep (fn [r] (when (and (= :file (resource/source-type r)) (resource/exists? r)) r)) resources))))

(defn- selection->single-resource-file [selection]
  (when-let [r (handler/adapt-single selection resource/Resource)]
    (when (= :file (resource/source-type r))
      r)))

(handler/defhandler :open :global
  (active? [selection user-data] (:resources user-data (not-empty (selection->resource-files selection))))
  (enabled? [selection user-data] (every? resource/exists? (:resources user-data (selection->resource-files selection))))
  (run [selection app-view workspace project user-data]
       (doseq [resource (:resources user-data (selection->resource-files selection))]
         (open-resource app-view workspace project resource))))

(handler/defhandler :open-as :global
  (active? [selection] (selection->single-resource-file selection))
  (enabled? [selection user-data] (resource/exists? (selection->single-resource-file selection)))
  (run [selection app-view workspace project user-data]
       (let [resource (selection->single-resource-file selection)]
         (open-resource app-view workspace project resource (when-let [view-type (:selected-view-type user-data)]
                                                              {:selected-view-type view-type}))))
  (options [workspace selection user-data]
           (when-not user-data
             (let [resource (selection->single-resource-file selection)
                   resource-type (resource/resource-type resource)]
               (map (fn [vt]
                      {:label     (or (:label vt) "External Editor")
                       :command   :open-as
                       :user-data {:selected-view-type vt}})
                    (:view-types resource-type))))))

(handler/defhandler :save-all :global
  (enabled? [] (not (project/ongoing-build-save?)))
  (run [project changes-view]
       (project/save-all! project #(changes-view/refresh! changes-view))))

(handler/defhandler :show-in-desktop :global
  (active? [selection] (selection->single-resource-file selection))
  (enabled? [selection] (when-let [r (selection->single-resource-file selection)]
                          (and (resource/abs-path r)
                               (resource/exists? r))))
  (run [selection] (when-let [r (selection->single-resource-file selection)]
                     (let [f (File. (resource/abs-path r))]
                       (.open (Desktop/getDesktop) (util/to-folder f))))))

(handler/defhandler :referencing-files :global
  (active? [selection] (selection->single-resource-file selection))
  (enabled? [selection] (when-let [r (selection->single-resource-file selection)]
                          (and (resource/abs-path r)
                               (resource/exists? r))))
  (run [selection app-view workspace project] (when-let [r (selection->single-resource-file selection)]
                                                (doseq [resource (dialogs/make-resource-dialog workspace project {:filter (format "refs:%s" (resource/proj-path r))})]
                                                  (open-resource app-view workspace project resource)))))

(handler/defhandler :dependencies :global
  (active? [selection] (selection->single-resource-file selection))
  (enabled? [selection] (when-let [r (selection->single-resource-file selection)]
                          (and (resource/abs-path r)
                               (resource/exists? r))))
  (run [selection app-view workspace project] (when-let [r (selection->single-resource-file selection)]
                                                (doseq [resource (dialogs/make-resource-dialog workspace project {:filter (format "deps:%s" (resource/proj-path r))})]
                                                  (open-resource app-view workspace project resource)))))

(defn- gen-tooltip [workspace project app-view resource]
  (let [resource-type (resource/resource-type resource)
        view-type (or (first (:view-types resource-type)) (workspace/get-view-type workspace :text))]
    (when-let [make-preview-fn (:make-preview-fn view-type)]
      (let [tooltip (Tooltip.)]
        (doto tooltip
          (.setGraphic (doto (ImageView.)
                         (.setScaleY -1.0)))
          (.setOnShowing (ui/event-handler
                           e
                           (let [image-view ^ImageView (.getGraphic tooltip)]
                             (when-not (.getImage image-view)
                               (let [resource-node (project/get-resource-node project resource)
                                         view-graph (g/make-graph! :history false :volatility 2)
                                         opts (assoc ((:id view-type) (:view-opts resource-type))
                                                     :app-view app-view
                                                     :project project
                                                     :workspace workspace)
                                         preview (make-preview-fn view-graph resource-node opts 256 256)]
                                     (.setImage image-view ^Image (g/node-value preview :image))
                                     (ui/user-data! image-view :graph view-graph))))))
          (.setOnHiding (ui/event-handler
                          e
                          (let [image-view ^ImageView (.getGraphic tooltip)]
                            (when-let [graph (ui/user-data image-view :graph)]
                              (g/delete-graph! graph))))))))))

(defn- make-resource-dialog [workspace project app-view]
  (when-let [resource (first (dialogs/make-resource-dialog workspace project {:tooltip-gen (partial gen-tooltip workspace project app-view)}))]
    (open-resource app-view workspace project resource)))

(handler/defhandler :select-items :global
  (run [user-data] (dialogs/make-select-list-dialog (:items user-data) (:options user-data))))

(handler/defhandler :open-asset :global
  (run [workspace project app-view] (make-resource-dialog workspace project app-view)))

(defn- make-search-in-files-dialog [workspace project app-view]
  (let [[resource opts] (dialogs/make-search-in-files-dialog project)]
    (when resource
      (open-resource app-view workspace project resource opts))))

(handler/defhandler :search-in-files :global
  (run [workspace project app-view] (make-search-in-files-dialog workspace project app-view)))

(defn- fetch-libraries [workspace project prefs]
  (future
    (ui/with-disabled-ui
      (ui/with-progress [render-fn ui/default-render-progress!]
        (let [library-urls (project/project-dependencies project)]
          (workspace/fetch-libraries! workspace library-urls render-fn (partial login/login prefs)))))))

(handler/defhandler :fetch-libraries :global
  (run [workspace project prefs] (fetch-libraries workspace project prefs)))

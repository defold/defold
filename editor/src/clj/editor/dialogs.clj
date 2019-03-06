(ns editor.dialogs
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [editor.ui.bindings :as b]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.util :as util]
            [editor.handler :as handler]
            [editor.core :as core]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.jfx :as jfx]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.defold-project :as project]
            [editor.github :as github])
  (:import [java.io File]
           [java.util List Collection]
           [java.nio.file Path Paths]
           [javafx.geometry Pos]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control CheckBox Button Label ListView TextArea TextField Hyperlink]
           [javafx.scene.input KeyCode]
           [javafx.scene.layout HBox VBox Region]
           [javafx.scene.text Text TextFlow]
           [javafx.stage Stage DirectoryChooser FileChooser FileChooser$ExtensionFilter Window]))

(set! *warn-on-reflection* true)

(defn ^:dynamic make-alert-dialog [text]
  (let [root ^Parent (ui/load-fxml "alert.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)]
    (ui/title! stage "Alert")
    (ui/with-controls root [^TextArea message ^Button ok]
      (ui/text! message text)
      (ui/on-action! ok (fn [_] (.close stage)))
      (.setDefaultButton ok true)
      (.setCancelButton ok true)
      (.setOnShown stage (ui/event-handler _ (.setScrollTop message 0.0))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-message-box [title text]
  (let [root ^Parent (ui/load-fxml "message.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)]
    (ui/title! stage title)
    (ui/with-controls root [message ^Button ok]
      (ui/text! message text)
      (.setDefaultButton ok true)
      (.setCancelButton ok true)
      (ui/on-action! ok (fn [_] (.close stage))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(def ^:private link-regex #"\[[^\]]+\]\([^)]+\)")
(def ^:private split-link-regex #"\[([^\]]+)\]\(([^)]+)\)")

(defn- split-link [link-str]
  (rest (re-find split-link-regex link-str)))

(defn- mark-matches [re s]
  (let [matcher (re-matcher re s)]
    (loop [matches []]
      (if (re-find matcher)
        (let [match {:start (.start matcher) :end (.end matcher)}]
          (recur (conj matches match)))
        matches))))

(defn- empty-run? [run]
  (= (:start run) (:end run)))

(defn- message->mark-runs
  [message]
  (let [links (mapv (fn [match]
                      (let [link (subs message (:start match) (:end match))
                            [label url] (rest (re-find split-link-regex link))]
                        (assoc match
                               :type :link
                               :label label
                               :url url)))
                    (mark-matches link-regex message))
        pseudo-start {:end 0}
        pseudo-end {:start (count message)}
        parts (concat [pseudo-start] links [pseudo-end])
        holes (map (fn [curr next] {:start (:end curr) :end (:start next)})
                   parts (rest parts))
        texts (map #(assoc %
                           :type :text
                           :text (subs message (:start %) (:end %)))
                   holes)
        runs (sort-by :start (into links texts))]
    (remove empty-run? runs)))

(defn- mark-run->node [run]
  (case (:type run)
    :link
    (let [{:keys [label url]} run]
      (doto (Hyperlink. label)
        (ui/add-style! "link-run")
        (ui/on-action! (fn [_] (ui/open-url url)))))

    :text
    (doto (Text. (:text run))
      (ui/add-style! "text-run"))))

(defn make-error-dialog
  ([title text] (make-error-dialog title text nil))
  ([title text detail-text]
   (let [root ^Parent (ui/load-fxml "error-dialog.fxml")
         stage (ui/make-dialog-stage)
         scene (Scene. root)]
     (.setResizable stage true) ; might want to resize if huge detail text
     (ui/context! root :dialog {:stage stage} nil)
     (ui/title! stage title)
     (ui/with-controls root [^TextFlow message ^CheckBox toggle-details ^TextArea details ^Button ok]
       (if-not (str/blank? detail-text)
         (do
           (b/bind-presence! details (.selectedProperty toggle-details))
           (ui/on-action! toggle-details (fn [_] (.sizeToScene stage)))
           (ui/text! details detail-text))
         (do
           (doto toggle-details
             (ui/visible! false)
             (ui/managed! false))
           (doto details
             (ui/visible! false)
             (ui/managed! false))))
       (let [runs (map mark-run->node (message->mark-runs text))]
         (ui/children! message runs))
       (ui/bind-action! ok ::close)
       (.setDefaultButton ok true)
       (.setCancelButton ok true)
       (ui/request-focus! ok))
     (.setScene stage scene)
     (ui/show-and-wait! stage))))

(defn make-confirm-dialog
  ([text]
   (make-confirm-dialog text {}))
  ([text options]
   (let [root ^Region (ui/load-fxml "confirm.fxml")
         stage (if-let [owner-window (:owner-window options)]
                 (ui/make-dialog-stage owner-window)
                 (ui/make-dialog-stage))
         scene (Scene. root)
         result-atom (atom false)]
     (ui/with-controls root [^Label message ^Button ok ^Button cancel]
       (ui/text! message text)
       (ui/text! ok (get options :ok-label "OK"))
       (ui/text! cancel (get options :cancel-label "Cancel"))
       (ui/on-action! ok (fn [_]
                           (reset! result-atom true)
                           (ui/close! stage)))
       (ui/on-action! cancel (fn [_]
                               (ui/close! stage))))
     (when-let [pref-width (:pref-width options)]
       (.setPrefWidth root pref-width))
     (ui/title! stage (get options :title "Please Confirm"))
     (.setScene stage scene)
     (ui/show-and-wait! stage)
     @result-atom)))

(defn make-update-failed-dialog [^Stage owner]
  (let [root ^Parent (ui/load-fxml "update-failed-alert.fxml")
        stage (ui/make-dialog-stage owner)
        scene (Scene. root)]
    (ui/title! stage "Update failed")
    (ui/with-controls root [^Button quit ^Button open-site]
      (ui/on-action! quit
        (fn [_]
          (ui/close! stage)))
      (ui/on-action! open-site
        (fn [_]
          (ui/open-url "https://www.defold.com/")
          (ui/close! stage))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-download-update-or-restart-dialog [^Stage owner]
  (let [root ^Parent (ui/load-fxml "update-or-restart-alert.fxml")
        stage (ui/make-dialog-stage owner)
        scene (Scene. root)
        result-atom (atom :cancel)
        make-action-fn (fn action! [result]
                         (fn [_]
                           (reset! result-atom result)
                           (ui/close! stage)))]
    (ui/title! stage "Update Available")
    (ui/with-controls root [^Button cancel ^Button restart ^Button download]
      (ui/on-action! cancel (make-action-fn :cancel))
      (ui/on-action! restart (make-action-fn :restart))
      (ui/on-action! download (make-action-fn :download)))
    (.setScene stage scene)
    (ui/show-and-wait! stage)
    @result-atom))

(defn make-platform-no-longer-supported-dialog [^Stage owner]
  (let [root ^Parent (ui/load-fxml "platform-no-longer-supported-alert.fxml")
        stage (ui/make-dialog-stage owner)
        scene (Scene. root)]
    (ui/title! stage "Update Available")
    (ui/with-controls root [^Button close]
      (ui/on-action! close (fn on-close! [_]
                             (ui/close! stage))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-download-update-dialog [^Stage owner]
  (let [root ^Parent (ui/load-fxml "update-alert.fxml")
        stage (ui/make-dialog-stage owner)
        scene (Scene. root)
        result-atom (atom false)]
    (ui/title! stage "Update Available")
    (ui/with-controls root [^Button ok ^Button cancel]
      (ui/on-action! ok (fn on-ok! [_]
                          (reset! result-atom true)
                          (ui/close! stage)))
      (ui/on-action! cancel (fn on-cancel! [_]
                              (ui/close! stage))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)
    @result-atom))

(handler/defhandler ::report-error :dialog
  (run [sentry-id-promise]
    (let [sentry-id (deref sentry-id-promise 100 nil)
          fields (cond-> {}
                   sentry-id
                   (assoc "Error" (format "<a href='https://sentry.io/defold/editor2/?query=id%%3A\"%s\"'>%s</a>"
                                          sentry-id sentry-id)))]
      (ui/open-url (github/new-issue-link fields)))))

(defn- messages
  [ex-map]
  (->> (tree-seq :via :via ex-map)
       (drop 1)
       (map (fn [{:keys [message ^Class type]}]
              (format "%s: %s" (.getName type) (or message "Unknown"))))
       (str/join "\n")))

(defn make-unexpected-error-dialog
  [ex-map sentry-id-promise]
  (let [root     ^Parent (ui/load-fxml "unexpected-error.fxml")
        stage    (ui/make-dialog-stage)
        scene    (Scene. root)
        controls (ui/collect-controls root ["message" "dismiss" "report"])]
    (ui/context! root :dialog {:stage stage :sentry-id-promise sentry-id-promise} nil)
    (ui/title! stage "Error")
    (ui/text! (:message controls) (messages ex-map))
    (ui/bind-action! (:dismiss controls) ::close)
    (.setCancelButton ^Button (:dismiss controls) true)
    (ui/bind-action! (:report controls) ::report-error)
    (.setDefaultButton ^Button (:report controls) true)
    (ui/request-focus! (:report controls))
    (.setScene stage scene)
    (ui/show-and-wait! stage)))

(defn make-gl-support-error-dialog [support-error]
  (let [root ^VBox (ui/load-fxml "gl-error.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        result (atom :quit)]
    (ui/with-controls root [message ^Button quit ^Button continue glgenbuffers-link opengl-linux-link]
      (when-not (util/is-linux?)
        (.. root getChildren (remove opengl-linux-link)))
      (ui/context! root :dialog {:stage stage} nil)
      (ui/title! stage "Insufficient OpenGL Support")
      (ui/text! message support-error)
      (ui/on-action! continue (fn [_] (reset! result :continue) (ui/close! stage)))
      (.setDefaultButton continue true)
      (ui/bind-action! quit ::close)
      (.setCancelButton quit true)
      (ui/on-action! glgenbuffers-link (fn [_] (ui/open-url (github/glgenbuffers-link))))
      (ui/on-action! opengl-linux-link (fn [_] (ui/open-url "https://www.defold.com/faq/#_linux_issues")))
      (.setScene stage scene)
      ;; We want to show this dialog before the main ui is up running
      ;; so we can't use ui/show-and-wait! which does some extra menu
      ;; update magic.
      (.showAndWait stage)
      @result)))

(defn make-file-dialog
  ^File [title filter-descs ^File initial-file ^Window owner-window]
  (let [chooser (FileChooser.)
        initial-directory (some-> initial-file .getParentFile)
        initial-file-name (some-> initial-file .getName)
        extension-filters (map (fn [filter-desc]
                                 (let [description ^String (first filter-desc)
                                       extensions ^List (vec (rest filter-desc))]
                                   (FileChooser$ExtensionFilter. description extensions)))
                               filter-descs)]
    (when (and (some? initial-directory) (.exists initial-directory))
      (.setInitialDirectory chooser initial-directory))
    (when (some? (not-empty initial-file-name))
      (.setInitialFileName chooser initial-file-name))
    (.addAll (.getExtensionFilters chooser) ^Collection extension-filters)
    (.setTitle chooser title)
    (.showOpenDialog chooser owner-window)))

(handler/defhandler ::confirm :dialog
  (enabled? [selection]
            (seq selection))
  (run [^Stage stage selection]
       (ui/user-data! stage ::selected-items selection)
       (ui/close! stage)))

(handler/defhandler ::close :dialog
  (run [^Stage stage]
       (ui/close! stage)))

(handler/defhandler ::focus :dialog
  (active? [user-data] (if-let [active-fn (:active-fn user-data)]
                         (active-fn nil)
                         true))
  (run [^Stage stage user-data]
       (when-let [^Node node (:node user-data)]
         (ui/request-focus! node))))

(defn- default-filter-fn [cell-fn text items]
  (let [text (str/lower-case text)
        str-fn (comp str/lower-case :text cell-fn)]
    (filter (fn [item] (str/starts-with? (str-fn item) text)) items)))

(defn make-select-list-dialog [items options]
  (let [^Parent root (ui/load-fxml "select-list.fxml")
        scene (Scene. root)
        ^Stage stage (doto (ui/make-dialog-stage (ui/main-stage))
                       (ui/title! (or (:title options) "Select Item"))
                       (.setScene scene))
        controls (ui/collect-controls root ["filter" "item-list" "ok"])
        ok-label (:ok-label options "OK")
        ^TextField filter-field (:filter controls)
        filter-value (or (:filter options)
                         (some-> (:filter-atom options) deref)
                         "")
        cell-fn (:cell-fn options identity)
        ^ListView item-list (doto ^ListView (:item-list controls)
                              (.setFixedCellSize 27.0) ; Fixes missing cells in VirtualFlow
                              (ui/cell-factory! cell-fn)
                              (ui/selection-mode! (:selection options :single)))]
    (doto item-list
      (ui/observe-list (ui/items item-list)
                       (fn [_ items]
                         (when (not (empty? items))
                           (ui/select-index! item-list 0))))
      (ui/items! (if (str/blank? filter-value) items [])))
    (let [filter-fn (or (:filter-fn options) (partial default-filter-fn cell-fn))]
      (ui/observe (.textProperty filter-field)
                  (fn [_ _ ^String new]
                    (let [filtered-items (filter-fn new items)]
                      (ui/items! item-list filtered-items)))))
    (doto filter-field
      (.setText filter-value)
      (.setPromptText (:prompt options "")))

    (ui/context! root :dialog {:stage stage} (ui/->selection-provider item-list))
    (ui/text! (:ok controls) ok-label)
    (ui/bind-action! (:ok controls) ::confirm)
    (ui/observe-selection item-list (fn [_ _] (ui/refresh-bound-action-enabled! (:ok controls))))
    (ui/bind-double-click! item-list ::confirm)
    (ui/bind-keys! root {KeyCode/ENTER ::confirm
                         KeyCode/ESCAPE ::close
                         KeyCode/DOWN [::focus {:active-fn (fn [_] (and (seq (ui/items item-list))
                                                                        (ui/focus? filter-field)))
                                                :node item-list}]
                         KeyCode/UP [::focus {:active-fn (fn [_] (= 0 (.getSelectedIndex (.getSelectionModel item-list))))
                                              :node filter-field}]})

    (ui/show-and-wait! stage)

    (let [selected-items (ui/user-data stage ::selected-items)
          filter-atom (:filter-atom options)]
      (when (and (some? selected-items) (some? filter-atom))
        (reset! filter-atom (.getText filter-field)))
      selected-items)))

(def ^:private fuzzy-resource-filter-fn (partial fuzzy-choices/filter-options resource/proj-path resource/proj-path))

(defn- override-seq [node-id]
  (tree-seq g/overrides g/overrides node-id))

(defn- file-scope [node-id]
  (last (take-while (fn [n] (and n (not (g/node-instance? project/Project n)))) (iterate core/scope node-id))))

(defn- refs-filter-fn [project filter-value items]
  ;; Temp limitation to avoid stalls
  ;; Optimally we would do the work in the background with a progress-bar
  (if-let [n (project/get-resource-node project filter-value)]
    (->>
      (let [all (override-seq n)]
        (mapcat (fn [n]
                  (keep (fn [[src src-label node-id label]]
                          (when-let [node-id (file-scope node-id)]
                            (when (and (not= n node-id)
                                       (g/node-instance? resource-node/ResourceNode node-id))
                              (when-let [r (g/node-value node-id :resource)]
                                (when (resource/exists? r)
                                  r)))))
                        (g/outputs n)))
                all))
      distinct)
    []))

(defn- sub-nodes [n]
  (g/node-value n :nodes))

(defn- sub-seq [n]
  (tree-seq (partial g/node-instance? resource-node/ResourceNode) sub-nodes n))

(defn- deps-filter-fn [project filter-value items]
  ;; Temp limitation to avoid stalls
  ;; Optimally we would do the work in the background with a progress-bar
  (if-let [node-id (project/get-resource-node project filter-value)]
    (->>
      (let [all (sub-seq node-id)]
        (mapcat
          (fn [n]
            (keep (fn [[src src-label tgt tgt-label]]
                    (when-let [src (file-scope src)]
                      (when (and (not= node-id src)
                                 (g/node-instance? resource-node/ResourceNode src))
                        (when-let [r (g/node-value src :resource)]
                          (when (resource/exists? r)
                            r)))))
                  (g/inputs n)))
          all))
      distinct)
    []))

(defn- make-text-run [text style-class]
  (let [text-view (Text. text)]
    (when (some? style-class)
      (.add (.getStyleClass text-view) style-class))
    text-view))

(defn- matched-text-runs [text matching-indices]
  (let [/ (or (some-> text (str/last-index-of \/) inc) 0)]
    (into []
          (mapcat (fn [[matched? start end]]
                    (cond
                      matched?
                      [(make-text-run (subs text start end) "matched")]

                      (< start / end)
                      [(make-text-run (subs text start /) "diminished")
                       (make-text-run (subs text / end) nil)]

                      (<= start end /)
                      [(make-text-run (subs text start end) "diminished")]

                      :else
                      [(make-text-run (subs text start end) nil)])))
          (fuzzy-text/runs (count text) matching-indices))))

(defn- make-matched-list-item-graphic [icon text matching-indices]
  (let [icon-view (jfx/get-image-view icon 16)
        text-view (TextFlow. (into-array Text (matched-text-runs text matching-indices)))]
    (doto (HBox. (ui/node-array [icon-view text-view]))
      (.setAlignment Pos/CENTER_LEFT)
      (.setSpacing 4.0))))

(defn make-resource-dialog [workspace project options]
  (let [exts         (let [ext (:ext options)] (if (string? ext) (list ext) (seq ext)))
        accepted-ext (if (seq exts) (set exts) (constantly true))
        accept-fn    (or (:accept-fn options) (constantly true))
        items        (into []
                           (filter #(and (= :file (resource/source-type %))
                                         (accepted-ext (resource/type-ext %))
                                         (not (resource/internal? %))
                                         (accept-fn %)))
                           (g/node-value workspace :resource-list))
        options (-> {:title "Select Resource"
                     :prompt "Type to filter"
                     :cell-fn (fn [r]
                                (let [text (resource/proj-path r)
                                      icon (workspace/resource-icon r)
                                      style (resource/style-classes r)
                                      tooltip (when-let [tooltip-gen (:tooltip-gen options)] (tooltip-gen r))
                                      matching-indices (:matching-indices (meta r))]
                                  (cond-> {:style style
                                           :tooltip tooltip}

                                          (empty? matching-indices)
                                          (assoc :icon icon :text text)

                                          :else
                                          (assoc :graphic (make-matched-list-item-graphic icon text matching-indices)))))
                     :filter-fn (fn [filter-value items]
                                  (let [fns {"refs" (partial refs-filter-fn project)
                                             "deps" (partial deps-filter-fn project)}
                                        [command arg] (let [parts (str/split filter-value #":")]
                                                        (if (< 1 (count parts))
                                                          parts
                                                          [nil (first parts)]))
                                        f (get fns command fuzzy-resource-filter-fn)]
                                    (f arg items)))}
                  (merge options))]
    (make-select-list-dialog items options)))

(declare sanitize-folder-name)

(defn make-new-folder-dialog [base-dir {:keys [validate]}]
  (let [root ^Parent (ui/load-fxml "new-folder-dialog.fxml")
        stage (ui/make-dialog-stage (ui/main-stage))
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "ok" "cancel" "path"])
        return (atom nil)
        reset-return! (fn [] (reset! return (some-> (ui/text (:name controls)) sanitize-folder-name not-empty)))
        close (fn [] (reset-return!) (.close stage))
        validate (or validate (constantly nil))
        do-validation (fn []
                        (let [sanitized (some-> (not-empty (ui/text (:name controls))) sanitize-folder-name)
                              validation-msg (some-> sanitized validate)]
                          (if (or (nil? sanitized) validation-msg)
                            (do (ui/text! (:path controls) (or validation-msg ""))
                                (ui/enable! (:ok controls) false))
                            (do (ui/text! (:path controls) sanitized)
                                (ui/enable! (:ok controls) true)))))]
    (ui/title! stage "New Folder")

    (ui/on-action! (:ok controls) (fn [_] (close)))
    (.setDefaultButton ^Button (:ok controls) true)
    (ui/on-action! (:cancel controls) (fn [_] (.close stage)))
    (.setCancelButton ^Button (:cancel controls) true)

    (ui/on-edit! (:name controls) (fn [_old _new] (do-validation)))

    (.setScene stage scene)

    (do-validation)

    (ui/show-and-wait! stage)

    @return))

(defn make-target-ip-dialog [ip msg]
  (let [root     ^Parent (ui/load-fxml "target-ip-dialog.fxml")
        stage    (ui/make-dialog-stage (ui/main-stage))
        scene    (Scene. root)
        controls (ui/collect-controls root ["add" "cancel" "ip" "msg"])
        return   (atom nil)]
    (ui/text! (:msg controls) (or msg "Enter Target IP address"))
    (ui/text! (:ip controls) ip)
    (ui/title! stage "Target IP")

    (ui/on-action! (:add controls)
                   (fn [_]
                     (reset! return (ui/text (:ip controls)))
                     (.close stage)))
    (.setDefaultButton ^Button (:add controls) true)
    (ui/on-action! (:cancel controls)
                   (fn [_] (.close stage)))
    (.setCancelButton ^Button (:cancel controls) true)

    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(defn- sanitize-common [name]
  (-> name
      str/trim
      (str/replace #"[/\\]" "") ; strip path separators
      (str/replace #"[\"']" "") ; strip quotes
      (str/replace #"^\.*" ""))) ; prevent hiding files (.dotfile)

(defn sanitize-file-name [extension name]
  (-> name
      sanitize-common
      (#(if (empty? extension) (str/replace % #"\..*" "" ) %)) ; disallow adding extension = resource type
      (#(if (and (seq extension) (seq %))
          (str % "." extension)
          %)))) ; append extension if there was one

(defn sanitize-folder-name [name]
  (sanitize-common name))

(defn make-rename-dialog ^String [name {:keys [title label validate sanitize] :as options}]
  (let [root     ^Parent (ui/load-fxml "rename-dialog.fxml")
        stage    (ui/make-dialog-stage (ui/main-stage))
        scene    (Scene. root)
        controls (ui/collect-controls root ["name" "path" "ok" "cancel" "name-label"])
        return   (atom nil)
        reset-return! (fn [] (reset! return (some-> (ui/text (:name controls)) sanitize not-empty)))
        close    (fn [] (reset-return!) (.close stage))
        validate (or validate (constantly nil))
        do-validation (fn []
                        (let [sanitized (some-> (not-empty (ui/text (:name controls))) sanitize)
                              validation-msg (some-> sanitized validate)]
                          (if (or (empty? sanitized) validation-msg)
                            (do (ui/text! (:path controls) (or validation-msg ""))
                                (ui/enable! (:ok controls) false))
                            (do (ui/text! (:path controls) sanitized)
                                (ui/enable! (:ok controls) true)))))]
    (ui/title! stage title)
    (when label
      (ui/text! (:name-label controls) label))
    (when-not (empty? name)
      (ui/text! (:path controls) (sanitize name))
      (ui/text! (:name controls) name)
      (.selectAll ^TextField (:name controls)))

    (ui/on-action! (:ok controls) (fn [_] (close)))
    (.setDefaultButton ^Button (:ok controls) true)
    (ui/on-action! (:cancel controls) (fn [_] (.close stage)))
    (.setCancelButton ^Button (:cancel controls) true)

    (ui/on-edit! (:name controls) (fn [_old _new] (do-validation)))

    (.setScene stage scene)

    (do-validation)

    (ui/show-and-wait! stage)

    @return))

(defn- relativize [^File base ^File path]
  (let [[^Path base ^Path path] (map #(Paths/get (.toURI ^File %)) [base path])]
    (str ""
         (when (.startsWith path base)
           (-> base
             (.relativize path)
             (.toString))))))

(defn make-new-file-dialog [^File base-dir ^File location type ext]
  (let [root ^Parent (ui/load-fxml "new-file-dialog.fxml")
        stage (ui/make-dialog-stage (ui/main-stage))
        scene (Scene. root)
        controls (ui/collect-controls root ["name" "location" "browse" "path" "ok" "cancel"])
        return (atom nil)
        close (fn [perform?]
                (when perform?
                  (reset! return (File. base-dir (ui/text (:path controls)))))
                (.close stage))
        set-location (fn [location] (ui/text! (:location controls) (relativize base-dir location)))]
    (ui/title! stage (str "New " type))
    (set-location location)

    (.bind (.textProperty ^TextField (:path controls))
      (.concat (.concat (.textProperty ^TextField (:location controls)) "/") (.concat (.textProperty ^TextField (:name controls)) (str "." ext))))

    (ui/on-action! (:browse controls) (fn [_] (let [location (-> (doto (DirectoryChooser.)
                                                                   (.setInitialDirectory (File. (str base-dir "/" (ui/text (:location controls)))))
                                                                   (.setTitle "Set Path"))
                                                               (.showDialog nil))]
                                                (when location
                                                  (set-location location)))))
    (ui/on-action! (:ok controls) (fn [_] (close true)))
    (.setDefaultButton ^Button (:ok controls) true)
    (ui/on-action! (:cancel controls) (fn [_] (close false)))
    (.setCancelButton ^Button (:cancel controls) true)

    (.setScene stage scene)
    (ui/show-and-wait! stage)

    @return))

(handler/defhandler ::rename-conflicting-files :dialog
  (run [^Stage stage]
    (ui/user-data! stage ::file-conflict-resolution-strategy :rename)
    (ui/close! stage)))

(handler/defhandler ::overwrite-conflicting-files :dialog
  (run [^Stage stage]
    (ui/user-data! stage ::file-conflict-resolution-strategy :overwrite)
    (ui/close! stage)))

(defn ^:dynamic make-resolve-file-conflicts-dialog
  [src-dest-pairs]
  (let [^Parent root (ui/load-fxml "resolve-file-conflicts.fxml")
        scene (Scene. root)
        ^Stage stage (doto (ui/make-dialog-stage (ui/main-stage))
                       (ui/title! "Name Conflict")
                       (.setScene scene))
        controls (ui/collect-controls root ["message" "rename" "overwrite" "cancel"])]
    (ui/context! root :dialog {:stage stage} nil)
    (ui/bind-action! (:rename controls) ::rename-conflicting-files)
    (ui/bind-action! (:overwrite controls) ::overwrite-conflicting-files)
    (ui/bind-action! (:cancel controls) ::close)
    (.setCancelButton ^Button (:cancel controls) true)
    (ui/text! (:message controls) (let [conflict-count (count src-dest-pairs)]
                                    (if (= 1 conflict-count)
                                      "The destination has an entry with the same name."
                                      (format "The destination has %d entries with conflicting names." conflict-count))))
    (ui/show-and-wait! stage)
    (ui/user-data stage ::file-conflict-resolution-strategy)))

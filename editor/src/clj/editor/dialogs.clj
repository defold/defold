(ns editor.dialogs
  (:require [cljfx.api :as fx]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.util :as util]
            [editor.handler :as handler]
            [editor.core :as core]
            [editor.fuzzy-text :as fuzzy-text]
            [editor.fxui :as fxui]
            [editor.jfx :as jfx]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.defold-project :as project]
            [editor.github :as github]
            [editor.field-expression :as field-expression])
  (:import [clojure.lang Named]
           [java.io File]
           [java.util List Collection]
           [java.nio.file Path Paths]
           [javafx.geometry Pos]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control Button ListView TextField]
           [javafx.scene.input KeyCode]
           [javafx.scene.layout HBox VBox]
           [javafx.scene.text Text TextFlow]
           [javafx.stage Stage DirectoryChooser FileChooser FileChooser$ExtensionFilter Window]))

(set! *warn-on-reflection* true)

(defn- dialog-stage
  "Dialog `:stage` that manages scene graph itself and provides layout common
  for many dialogs.

  Scene graph is configured using these keys:
  - `:size` (optional, default `:default`) - a dialog width, either `:small`,
    `:default` or `:large`
  - `:header` (required, fx description) - header of a dialog, padded
  - `:content` (optional, nil allowed) - content of a dialog, not padded, you
    can use \"dialog-content-padding\" style class to set desired padding (or
    \"text-area-with-dialog-content-padding\" for text areas)
  - `:footer` (required, fx description) - footer of a dialog, padded"
  [{:keys [size header content footer]
    :or {size :default}
    :as props}]
  (-> props
      (dissoc :size :header :content :footer)
      (assoc :fx/type fxui/dialog-stage
             :scene {:fx/type :scene
                     :stylesheets ["dialogs.css"]
                     :root {:fx/type :v-box
                            :style-class ["dialog-body" (case size
                                                          :small "dialog-body-small"
                                                          :default "dialog-body-default"
                                                          :large "dialog-body-large")]
                            :children (if (some? content)
                                        [{:fx/type :v-box
                                          :style-class "dialog-with-content-header"
                                          :children [header]}
                                         {:fx/type :v-box
                                          :style-class "dialog-content"
                                          :children [content]}
                                         {:fx/type :v-box
                                          :style-class "dialog-with-content-footer"
                                          :children [footer]}]
                                        [{:fx/type :v-box
                                          :style-class "dialog-without-content-header"
                                          :children [header]}
                                         {:fx/type :region :style-class "dialog-no-content"}
                                         {:fx/type :v-box
                                          :style-class "dialog-without-content-footer"
                                          :children [footer]}])}})))

(defn- confirmation-dialog-header->fx-desc [header]
  (if (string? header)
    {:fx/type fxui/label
     :variant :header
     :text header}
    header))

(defn- dialog-buttons [props]
  (-> props
      (assoc :fx/type :h-box)
      (fxui/provide-defaults :alignment :center-right)
      (update :style-class fxui/add-style-classes "spacing-smaller")))

(defn- confirmation-dialog [{:keys [buttons icon]
                             :or {icon ::no-icon}
                             :as props}]
  (let [button-descs (mapv (fn [button-props]
                             (let [button-desc (-> button-props
                                                   (assoc :fx/type fxui/button
                                                          :on-action {:result (:result button-props)})
                                                   (dissoc :result))]
                               (if (:default-button button-props)
                                 {:fx/type fxui/ext-focused-by-default
                                  :desc (fxui/provide-defaults button-desc :variant :primary)}
                                 button-desc)))
                           buttons)]
    (-> props
        (assoc :fx/type dialog-stage
               :showing (fxui/dialog-showing? props)
               :footer {:fx/type dialog-buttons
                        :children button-descs}
               :on-close-request {:result (:result (some #(when (:cancel-button %) %) buttons))})
        (dissoc :buttons :icon :result)
        (update :header (fn [header]
                          (let [header-desc (confirmation-dialog-header->fx-desc header)]
                            (if (= icon ::no-icon)
                              header-desc
                              {:fx/type :h-box
                               :style-class "spacing-smaller"
                               :alignment :center-left
                               :children [{:fx/type fxui/icon :type icon}
                                          header-desc]})))))))

(defn make-confirmation-dialog
  "Shows a dialog and blocks current thread until users selects one option.

  `props` is a prop map for `editor.dialogs/dialog-stage`, but instead of
  `:footer` you use `:buttons`

  Additional keys:
  - `:buttons` (optional) - a coll of button descriptions. Button
  description is a prop map for `editor.fxui/button` with few caveats:
    * you don't have to specify `:fx/type`
    * it should have `:result` key, it's value will be returned from this
      function (default `nil`)
    * if you specify `:default-button`, it will be styled as primary and receive
      focus by default
    * if you specify `:cancel-button`, closing window using `x` button will
      return `:result` from such button (and `nil` otherwise)
  - `:icon` (optional) - a keyword valid as `:type` for `editor.fxui/icon`, if
    present, will add an icon to the left of a header"
  [props]
  (fxui/show-dialog-and-await-result!
    :event-handler (fn [state event]
                     (assoc state :result (:result event)))
    :description (assoc props :fx/type confirmation-dialog)))

(defn- info-dialog-text-area [props]
  (-> props
      (assoc :fx/type fxui/text-area)
      (update :style-class fxui/add-style-classes "text-area-with-dialog-content-padding")
      (fxui/provide-defaults
        :pref-row-count (max 3 (count (string/split (:text props "") #"\n" 10)))
        :variant :borderless
        :editable false)))

(defn make-info-dialog
  "Shows a dialog with selectable text content and blocks current thread until
  user closes it.

  `props` is a map to configure such dialog, supports all options from
  `editor.dialogs/make-confirmation-dialog` with these changes:
  - `:buttons` have a close button by default
  - `:content` can be:
    * fx description (a map with `:fx/type` key) - used as is
    * prop map (map without `:fx/type` key) for `editor.fxui/text-area` -
      readonly by default to allow user select and copy text, `:text` prop is
      required
    * string - text for readonly text area"
  [props]
  (make-confirmation-dialog
    (-> props
        (update :content (fn [content]
                           (cond
                             (:fx/type content)
                             content

                             (map? content)
                             (assoc content :fx/type info-dialog-text-area)

                             (string? content)
                             {:fx/type info-dialog-text-area :text content})))
        (fxui/provide-defaults :buttons [{:text "Close"
                                          :cancel-button true
                                          :default-button true}]))))

(defn- digit-string? [^String x]
  (and (pos? (.length x))
       (every? #(Character/isDigit ^char %) x)))

(defn fx-resolution-dialog [{:keys [width-text height-text] :as props}]
  (let [width-valid (digit-string? width-text)
        height-valid (digit-string? height-text)]
    {:fx/type dialog-stage
     :showing (not (contains? props :result))
     :on-close-request {:event-type :cancel}
     :title "Set Custom Resolution"
     :size :small
     :header {:fx/type :v-box
              :children [{:fx/type fxui/label
                          :variant :header
                          :text "Set custom game resolution"}
                         {:fx/type fxui/label
                          :text "Game window will be resized to this size"}]}
     :content {:fx/type fxui/two-col-input-grid-pane
               :style-class "dialog-content-padding"
               :children [{:fx/type fxui/label
                           :text "Width"}
                          {:fx/type fxui/text-field
                           :variant (if width-valid :default :error)
                           :text width-text
                           :on-text-changed {:event-type :set-width}}
                          {:fx/type fxui/label
                           :text "Height"}
                          {:fx/type fxui/text-field
                           :variant (if height-valid :default :error)
                           :text height-text
                           :on-text-changed {:event-type :set-height}}]}
     :footer {:fx/type dialog-buttons
              :children [{:fx/type fxui/button
                          :cancel-button true
                          :on-action {:event-type :cancel}
                          :text "Cancel"}
                         {:fx/type fxui/button
                          :variant :primary
                          :disable (or (not width-valid) (not height-valid))
                          :default-button true
                          :text "Set Resolution"
                          :on-action {:event-type :confirm}}]}}))

(defn make-resolution-dialog []
  (fxui/show-dialog-and-await-result!
    :initial-state {:width-text "320"
                    :height-text "420"}
    :event-handler (fn [state {:keys [fx/event event-type]}]
                     (case event-type
                       :set-width (assoc state :width-text event)
                       :set-height (assoc state :height-text event)
                       :cancel (assoc state :result nil)
                       :confirm (assoc state :result {:width (field-expression/to-int (:width-text state))
                                                      :height (field-expression/to-int (:height-text state))})))
    :description {:fx/type fx-resolution-dialog}))

(defn make-update-failed-dialog [^Stage owner]
  (let [result (make-confirmation-dialog
                 {:title "Update Failed"
                  :owner owner
                  :icon :icon/error-triangle
                  :header {:fx/type :v-box
                           :children [{:fx/type fxui/label
                                       :variant :header
                                       :text "An error occurred during update installation"}
                                      {:fx/type fxui/label
                                       :text "You probably should perform a fresh install"}]}
                  :buttons [{:text "Quit"
                             :cancel-button true
                             :result false}
                            {:text "Open defold.com"
                             :default-button true
                             :result true}]})]
    (when result
      (ui/open-url "https://www.defold.com/"))
    result))

(defn make-download-update-or-restart-dialog [^Stage owner]
  (make-confirmation-dialog
    {:title "Update Available"
     :icon :icon/info-circle
     :size :large
     :owner owner
     :header {:fx/type :v-box
              :children [{:fx/type fxui/label
                          :variant :header
                          :text "Update is ready, but there is even newer version available"}
                         {:fx/type fxui/label
                          :text "You can install downloaded update or download newer one"}]}
     :buttons [{:text "Not Now"
                :cancel-button true
                :result :cancel}
               {:text "Install and Restart"
                :result :restart}
               {:text "Download Newer Version"
                :result :download}]}))

(defn make-platform-no-longer-supported-dialog [^Stage owner]
  (make-confirmation-dialog
    {:title "Platform not supported"
     :icon :icon/circle-sad
     :owner owner
     :header {:fx/type :v-box
              :children [{:fx/type fxui/label
                          :variant :header
                          :text "Updates are no longer provided for this platform"}
                         {:fx/type fxui/label
                          :text "Supported platforms are 64-bit Linux, macOS and Windows"}]}
     :buttons [{:text "Close"
                :cancel-button true
                :default-button true}]}))

(defn make-download-update-dialog [^Stage owner]
  (make-confirmation-dialog
    {:title "Update Available"
     :header "A newer version of Defold is available!"
     :icon :icon/info-circle
     :owner owner
     :buttons [{:text "Not Now"
                :cancel-button true
                :result false}
               {:text "Download Update"
                :default-button true
                :result true}]}))

(defn- messages
  [ex-map]
  (->> (tree-seq :via :via ex-map)
       (drop 1)
       (map (fn [{:keys [message type]}]
              (let [type-name (cond
                                (instance? Class type) (.getName ^Class type)
                                (instance? Named type) (name type)
                                :else (str type))]
                (format "%s: %s" type-name (or message "Unknown")))))
       (string/join "\n")))

(defn unexpected-error-dialog-fx [{:keys [ex-map] :as props}]
  {:fx/type dialog-stage
   :showing (fxui/dialog-showing? props)
   :on-close-request {:result false}
   :title "Error"
   :header {:fx/type :h-box ;; TODO vlaaad DRY?
            :style-class "spacing-smaller"
            :alignment :center-left
            :children [{:fx/type fxui/icon
                        :type :icon/error-triangle}
                       {:fx/type fxui/label
                        :variant :header
                        :text "An error occurred"}]}
   :content {:fx/type info-dialog-text-area
             :text (messages ex-map)}
   :footer {:fx/type :v-box
            :style-class "spacing-smaller"
            :children [{:fx/type fxui/label
                        :text "You can help us fix this problem by reporting it and providing more information about what you were doing when it happened."}
                       {:fx/type dialog-buttons
                        :children [{:fx/type fxui/button
                                    :cancel-button true
                                    :on-action {:result false}
                                    :text "Dismiss"}
                                   {:fx/type fxui/ext-focused-by-default
                                    :desc {:fx/type fxui/button
                                           :variant :primary
                                           :default-button true
                                           :on-action {:result true}
                                           :text "Report"}}]}]}})

(defn make-unexpected-error-dialog [ex-map sentry-id-promise]
  (when (fxui/show-dialog-and-await-result!
          :event-handler (fn [state event]
                           (assoc state :result (:result event)))
          :description {:fx/type unexpected-error-dialog-fx
                        :ex-map ex-map})
    (let [sentry-id (deref sentry-id-promise 100 nil)
          fields (if sentry-id
                   {"Error" (format "<a href='https://sentry.io/defold/editor2/?query=id%%3A\"%s\"'>%s</a>"
                                    sentry-id sentry-id)}
                   {})]
      (ui/open-url (github/new-issue-link fields)))))

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
  (let [text (string/lower-case text)
        str-fn (comp string/lower-case :text cell-fn)]
    (filter (fn [item] (string/starts-with? (str-fn item) text)) items)))

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
      (ui/items! (if (string/blank? filter-value) items [])))
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
  (let [/ (or (some-> text (string/last-index-of \/) inc) 0)]
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
                                        [command arg] (let [parts (string/split filter-value #":")]
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

(defn target-ip-dialog-fx [{:keys [msg ^String ip] :as props}]
  (let [ip-valid (pos? (.length ip))]
    {:fx/type dialog-stage
     :showing (fxui/dialog-showing? props)
     :on-close-request {:event-type :cancel}
     :title "Enter Target IP"
     :size :small
     :header {:fx/type fxui/label
              :variant :header
              :text msg}
     :content {:fx/type fxui/two-col-input-grid-pane
               :style-class "dialog-content-padding"
               :children [{:fx/type fxui/label
                           :text "Target IP Address"}
                          {:fx/type fxui/text-field
                           :variant (if ip-valid :default :error)
                           :text ip
                           :on-text-changed {:event-type :set-ip}}]}
     :footer {:fx/type dialog-buttons
              :children [{:fx/type fxui/button
                          :text "Cancel"
                          :cancel-button true
                          :on-action {:event-type :cancel}}
                         {:fx/type fxui/button
                          :disable (not ip-valid)
                          :text "Add Target IP"
                          :variant :primary
                          :default-button true
                          :on-action {:event-type :confirm}}]}}))

(defn make-target-ip-dialog [ip msg]
  (fxui/show-dialog-and-await-result!
    :initial-state {:ip (or ip "")}
    :event-handler (fn [state {:keys [fx/event event-type]}]
                     (case event-type
                       :set-ip (assoc state :ip event)
                       :cancel (assoc state :result nil)
                       :confirm (assoc state :result (:ip state))))
    :description {:fx/type target-ip-dialog-fx
                  :msg (or msg "Enter Target IP Address")}))

(defn- sanitize-common [name]
  (-> name
      string/trim
      (string/replace #"[/\\]" "") ; strip path separators
      (string/replace #"[\"']" "") ; strip quotes
      (string/replace #"^\.*" ""))) ; prevent hiding files (.dotfile)

(defn sanitize-file-name [extension name]
  (-> name
      sanitize-common
      (#(if (empty? extension) (string/replace % #"\..*" "") %)) ; disallow adding extension = resource type
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

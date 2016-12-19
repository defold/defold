(ns editor.form-view
  (:require [dynamo.graph :as g]
            [editor.form :as form]
            [editor.field-expression :as field-expression]
            [editor.ui :as ui]
            [editor.jfx :as jfx]            
            [editor.dialogs :as dialogs]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [editor.types :as types])
  (:import [javafx.animation AnimationTimer]
           [java.util List Collection]
           [javafx.scene Parent Group Node]
           [javafx.scene.text Text]
           [javafx.scene.input KeyCode KeyEvent ContextMenuEvent]
           [javafx.geometry Insets Pos HPos VPos]
           [javafx.util Callback StringConverter]
           [javafx.collections FXCollections ObservableList]
           [javafx.beans.property ReadOnlyObjectWrapper]
           [javafx.beans.value ChangeListener]
           [javafx.beans.binding Bindings]
           [javafx.scene.layout Region AnchorPane Pane GridPane HBox VBox Priority ColumnConstraints]
           [javafx.scene.control Control Cell ListView ListView$EditEvent TableView TableColumn TableColumn$CellDataFeatures TableColumn$CellEditEvent ScrollPane TextArea Label TextField ComboBox CheckBox Button Tooltip ContextMenu Menu MenuItem]
           [com.defold.control ListCell TableCell]))

(set! *warn-on-reflection* true)

(def grid-hgap 4)
(def grid-vgap 6)

(defmulti create-field-control (fn [field-info field-ops ctxt] (:type field-info)))

;; FIXME: When using field controls for cell editing, we would really like to add a ui/on-focus! and set the new val when focus is lost, but due to a javafx bug the
;; edit is cancelled and the value never committed.
;; see:  https://bugs.openjdk.java.net/browse/JDK-8089514
;; possible workaround: http://stackoverflow.com/questions/24694616/how-to-enable-commit-on-focuslost-for-tableview-treetableview

(def ^:private default-value-alignments
  {:number Pos/CENTER_RIGHT
   :integer Pos/CENTER_RIGHT
   :boolean Pos/CENTER
   :vec4 Pos/CENTER})

(defn- value-alignment [field-info]
  (get default-value-alignments (:type field-info) Pos/CENTER_LEFT))

(def ^:private default-field-widths
  {:number 80
   :integer 80
   :string 300
   :resource 300
   :list 300})

(defn- field-width [field-info]
  (get default-field-widths (:type field-info) 100))

(defn- install-escape-handler! [ctrl cancel]
  (when cancel
    (ui/on-key! ctrl (fn [key]
                       (when (= key KeyCode/ESCAPE)
                         (cancel))))))

(defn- create-text-field-control [parse serialize {:keys [path help] :as field-info} {:keys [set cancel]}]
  (let [tf (TextField.)]
    (.setPrefWidth tf (field-width field-info))
    (.setMinWidth tf Control/USE_PREF_SIZE)
    (ui/on-action! tf (fn [_]
                         (when-let [val (parse (ui/text tf))]
                           (set path val))))
    (install-escape-handler! tf cancel)
;; See FIXME above.
;;     (ui/on-focus! tf (fn [got-focus]
;;                        (when (not got-focus)
;;                          (when-let [val (parse (ui/text tf))]
;;                            (set path val)))))
    (.setAlignment tf (value-alignment field-info))
    (ui/tooltip! tf help)
    [tf {:update #(ui/text! tf (serialize %))
         :edit #(doto tf (.requestFocus) (.selectAll))}]))

(defmethod create-field-control :string [field-info field-ops ctxt]
  (create-text-field-control identity identity field-info field-ops))

(defmethod create-field-control :integer [field-info field-ops ctxt]
  (create-text-field-control field-expression/to-int str field-info field-ops))

(defmethod create-field-control :number [field-info field-ops ctxt]
  (create-text-field-control field-expression/to-double str field-info field-ops))

(defmethod create-field-control :boolean [{:keys [path help]} {:keys [set cancel]} ctxt]
  (let [check (CheckBox.)
        update-fn (fn [value]
                    (if (nil? value)
                      (.setIndeterminate check true)
                      (doto check
                        (.setIndeterminate false)
                        (.setSelected value))))]

    (ui/on-action! check (fn [_] (set path (.isSelected check))))
    (install-escape-handler! check cancel)
;; See FIXME above.
;;     (ui/on-focus! check (fn [got-focus]
;;                           (when (not got-focus)
;;                             (set path (.isSelected check)))))
    (ui/tooltip! check help)
    [check {:update update-fn
            :edit #(ui/request-focus! check)}]))

(defmethod create-field-control :choicebox [{:keys [options path help]} {:keys [set cancel]} _]
  (let [option-map (into {} options)
        inv-options (clojure.set/map-invert option-map)
        converter (proxy [StringConverter] []
                              (toString [value]
                                (get option-map value (str value)))
                              (fromString [s]
                                (inv-options s)))
        internal-change (atom false)
        cb (doto (ComboBox.)
             (-> (.getItems) (.addAll (object-array (map first options))))
             (.setConverter converter)
             (ui/cell-factory! (fn [val] {:text (option-map val)})))
        update-fn (fn [value]
                    (reset! internal-change true)
                    (.setValue cb value)
                    (reset! internal-change false))]
    (ui/observe (.valueProperty cb)
                (fn [observable old-val new-val]
                  (when-not @internal-change
                    (set path new-val))))
    (install-escape-handler! cb cancel)
;; See FIXME above.
;;     (ui/on-focus! cb (fn [got-focus]
;;                        (when (not got-focus)
;;                          (set path (.getValue cb)))))
    (ui/tooltip! cb help)
    [cb {:update update-fn
         :edit #(do (ui/request-focus! cb) (.show cb))}]))

(defmethod create-field-control :resource [{:keys [filter path help] :as field-info} {:keys [set cancel]} {:keys [workspace project]}]
  (let [box (GridPane.)
        browse-button (doto (Button. "\u2026") ; "..." (HORIZONTAL ELLIPSIS)
                        (.setPrefWidth 26)
                        (ui/add-style! "button-small"))
        open-button (doto (Button. "" (jfx/get-image-view "icons/32/Icons_S_14_linkarrow.png" 16))
                      (.setMaxWidth 26)
                       (ui/add-style! "button-small"))
        text (let [tf (TextField.)]
               (.setPrefWidth tf (field-width field-info))
               (.setMinWidth tf Control/USE_PREF_SIZE)
               (GridPane/setFillWidth tf true)
               tf)
        update-fn (fn [value] (ui/text! text value))]
    (ui/add-style! box "composite-property-control-container")
    (ui/on-action! browse-button (fn [_] (when-let [resource (first (dialogs/make-resource-dialog workspace project {:ext (when filter [filter])}))]
                                           (set path resource))))
    (ui/on-action! open-button (fn [_] (when-let [resource (workspace/resolve-workspace-resource workspace (workspace/to-absolute-path (ui/text text)))]
                                         (ui/run-command open-button :open {:resources [resource]}))))
    (ui/on-action! text (fn [_] (let [rpath (workspace/to-absolute-path (ui/text text))
                                      resource (workspace/resolve-workspace-resource workspace rpath)]
                                  (when-let [resource-path (and resource (resource/proj-path resource))]
                                    (set path resource-path)
                                    (update-fn resource-path)))))
    (install-escape-handler! text cancel)
    (ui/children! box [text open-button browse-button])
    (GridPane/setConstraints text 0 0)
    (GridPane/setConstraints open-button 1 0)
    (GridPane/setConstraints browse-button 2 0)

    ; Merge the facing borders of the open and browse buttons.
    (GridPane/setMargin open-button (Insets. 0 -1 0 0))
    (.setOnMousePressed open-button (ui/event-handler _ (.toFront open-button)))
    (.setOnMousePressed browse-button (ui/event-handler _ (.toFront browse-button)))

    (doto (.. box getColumnConstraints)
      (.add (doto (ColumnConstraints.)
              (.setHgrow Priority/ALWAYS)))
      (.add (doto (ColumnConstraints.)
              (.setMinWidth ColumnConstraints/CONSTRAIN_TO_PREF)
              (.setHgrow Priority/NEVER)))
      (.add (doto (ColumnConstraints.)
              (.setMinWidth ColumnConstraints/CONSTRAIN_TO_PREF)
              (.setHgrow Priority/NEVER))))

    (ui/tooltip! text help)
    
    [box {:update update-fn
          :edit #(ui/request-focus! text)}]))

(defn- create-field-component [ctrls]
  (let [box (doto (GridPane.)
              (ui/add-style! "property-component")
              (ui/children! ctrls))]
    (doall (map-indexed (fn [idx c]
                          (GridPane/setConstraints c idx 0))
                        ctrls))
    box))

(defn- create-multi-number-field-control [labels {:keys [path help] :as field-info} {:keys [set cancel]}]
  (let [text-fields (mapv (fn [l]
                            (let [tf (TextField.)]
                              (.setMinWidth tf (field-width field-info))
                              (GridPane/setFillWidth tf true)
                              tf))
                            labels)
        box (doto (GridPane.)
              (.setHgap grid-hgap))
        current-value (atom nil)
        update-fn (fn [value]
                    (reset! current-value value)
                    (doseq [[tf value-part] (zipmap text-fields value)]
                      (ui/text! tf (str value-part))))
        parse-val (fn []
                    (let [val (mapv (comp field-expression/to-double ui/text) text-fields)]
                      (when (not (some #{nil} val))
                        val)))
        setter (fn [] (if-let [val (parse-val)]
                        (do
                          (reset! current-value val)
                          (set path @current-value))
                        (update-fn @current-value)))]
    (doseq [tf text-fields]
      (ui/on-action! tf (fn [_] (setter)))
      (install-escape-handler! tf cancel)
;; See FIXME above.
;;       (ui/on-focus! tf (fn [got-focus]
;;                          (when (not got-focus)
;;                            (when-let [focd (some-> tf .getScene .getFocusOwner)]
;;                              (when (not (some #{focd} text-fields))
;;                                (setter))))))
      (ui/tooltip! tf help))
    (doall (map-indexed (fn [idx [tf label]]
                          (let [label-ctrl (doto (Label. label)
                                             (.setMinWidth 25))
                                comp (create-field-component [label-ctrl tf])]
                            (ui/add-child! box comp)
                            (GridPane/setConstraints comp idx 0)
                            (GridPane/setFillWidth comp true)))
                        (map vector text-fields labels)))

    [box {:update update-fn
          :edit #(doto ^TextField (first text-fields) (.requestFocus) (.selectAll))}]))

(defmethod create-field-control :vec4 [field-info field-ops _]
  (create-multi-number-field-control ["x" "y" "z" "w"] field-info field-ops))

(defmulti get-value-string-fn (fn [field-info] (:type field-info)))

(defmethod get-value-string-fn :choicebox [field-info]
  (let [option-map (into {} (:options field-info))]
    (fn [value]
      (or (option-map value)
          (name value)))))

(defmethod get-value-string-fn :default [_]
  str)

(defn- create-cell-field-control [^Cell cell column-info ctxt]
  (let [field-ops {:set (fn [_ value] (.commitEdit cell value))
                   :cancel #(.cancelEdit cell)
                   }
        [^Control control api] (create-field-control column-info field-ops ctxt)] ; column-info ~ field-info
    (GridPane/setConstraints control 0 0)
    (GridPane/setFillWidth control true)
    (ui/fill-control control)
    [(AnchorPane. (into-array Node [control])) api]))

(defn- resize-column-to-fit-control [^TableColumn table-column ^Node ctrl]
  (let [min-width (.minWidth ctrl -1)
        curr-width (.getWidth table-column)]
    (when (< curr-width min-width)
      (.impl_setWidth table-column min-width))))

(defn- create-table-cell-factory [column-info ctxt]
  (reify Callback
    (call [this p]
      (let [ctrl-data (atom nil)
            get-value-string (get-value-string-fn column-info)
            tc (proxy [TableCell] []
                 (startEdit []
                   (let [this ^TableCell this]
                     (when (not (.isEmpty this))
                       (proxy-super startEdit)
                       (reset! ctrl-data (create-cell-field-control this column-info ctxt))
                       ((:update (second @ctrl-data)) (.getItem this))
                       (ui/add-style! this "editing-cell")
                       (.setText this nil)
                       (.setGraphic this (first @ctrl-data))
                       (let [start-edit (:edit (second @ctrl-data))
                             table-column (.getTableColumn this)]
                       (ui/run-later
                        (resize-column-to-fit-control table-column (first @ctrl-data))
                        (when [start-edit]
                          (start-edit)))))))
                 (cancelEdit []
                   (let [this ^TableCell this]
                     (proxy-super cancelEdit)
                     (ui/remove-style! this "editing-cell")
                     (.setText this (get-value-string (.getItem this)))
                     (.setGraphic this nil)))
                 (updateItem [item empty]
                   (let [this ^TableCell this]
                     (proxy-super updateItem item empty)
                     (if empty
                       (do (.setText this nil)
                           (.setGraphic this nil))
                       (do (if (.isEditing this)
                             (do
                               (when @ctrl-data
                                 ((:update (second @ctrl-data)) (.getItem this)))
                               (.setText this nil)
                               (.setGraphic this (first @ctrl-data)))
                             (do
                               (ui/remove-style! this "editing-cell")
                               (.setText this (get-value-string (.getItem this)))
                               (.setGraphic this nil))))))))]
        (doto tc
          (.setAlignment (value-alignment column-info)))))))

(defn- create-cell-value-factory [column-info]
  (reify Callback
    (call [this p]
      (ReadOnlyObjectWrapper. (((comp last :path) column-info) (.getValue ^TableColumn$CellDataFeatures p))))))

(defn- create-table-column [column-info cell-setter ctxt]
  (let [table-column (TableColumn. (:label column-info))]
    (.setCellValueFactory table-column (create-cell-value-factory column-info))
    (.setCellFactory table-column (create-table-cell-factory column-info ctxt))
    (.setOnEditCommit table-column
                      (ui/event-handler event
                                        (let [event ^TableColumn$CellEditEvent event]
                                          (cell-setter (-> event .getTablePosition .getRow)
                                                       (:path column-info)
                                                       (.getNewValue event)))))
    table-column))

(defn- menu-item [label action]
  (doto (MenuItem. label)
    (.setOnAction (ui/event-handler event (action)))))


(defn- remove-vec-index [v index]
  (vec (concat (subvec v 0 index) (subvec v (inc index)))))

(defn- remove-table-row [table-data row]
  (remove-vec-index table-data row))

(defn- set-context-menu! [^Control ctrl item-descs]
  (.addEventHandler ctrl ContextMenuEvent/CONTEXT_MENU_REQUESTED
                    (ui/event-handler event
                                      (let [event ^ContextMenuEvent event]
                                        (when-not (.isConsumed event)
                                          (let [cm (ContextMenu.)]
                                            (.addAll (.getItems cm)
                                                     (to-array
                                                      (keep identity
                                                            (map (fn [[label action enabled]]
                                                                   (when (or (nil? enabled) (enabled))
                                                                     (menu-item label action)))
                                                                 item-descs))))
                                            (.setImpl_showRelativeToWindow cm true)
                                            (.show cm ctrl (.getScreenX event) (.getScreenY event))
                                            (.consume event)))))))

(defmethod create-field-control :table [field-info {:keys [set cancel] :as field-ops} ctxt]
  (assert (not cancel) "no support for nested tables")
  (let [table (TableView.)
        content (atom nil)
        update-fn (fn [value]
                    (reset! content (if (seq value) value []))
                    (.setAll (.getItems table) ^Collection @content))
        cell-setter (fn [row path val]
                      (swap! content
                             assoc-in (cons row path) val)
                      (set (:path field-info) @content))
        default-row (form/table-row-defaults field-info)
        on-add-row (fn []
                     (swap! content conj default-row)
                     (set (:path field-info) @content))
        selected-row (fn [] (let [selection-model (.getSelectionModel table)]
                              (when (not (.isEmpty selection-model))
                                (first (.getSelectedIndices selection-model)))))
        on-remove-row (fn []
                        (swap! content remove-table-row (selected-row))
                        (set (:path field-info) @content))]

    (.setFixedCellSize table 26)
    (.bind (.prefHeightProperty table)
           (.add (.multiply (Bindings/size (.getItems table))
                            (.getFixedCellSize table))
                 52))

    (.setEditable table true)
    (.setAll (.getColumns table)
             ^Collection (map (fn [column-info] (create-table-column column-info cell-setter ctxt)) (:columns field-info)))

    (set-context-menu! table [["Add" on-add-row (constantly default-row)]
                              ["Remove" on-remove-row selected-row]])

    (GridPane/setFillWidth table true)

    [table {:update update-fn}]))

(defn- create-list-cell-factory [element-info ctxt]
  (let [get-value-string (get-value-string-fn element-info)]
    (reify Callback
      (call ^ListCell [this view]
        (let [ctrl-data (atom nil)]
          (proxy [ListCell] []
            (startEdit []
              (let [this ^ListCell this]
                (when (not (.isEmpty this))
                  (proxy-super startEdit)
                  (reset! ctrl-data (create-cell-field-control this element-info ctxt))
                  ((:update (second @ctrl-data)) (.getItem this))
                  (ui/add-style! this "editing-cell")
                  (.setText this nil)
                  (.setGraphic this (first @ctrl-data))
                  (when-let [start-edit (:edit (second @ctrl-data))]
                    (ui/run-later
                     (start-edit))))))
            (cancelEdit []
              (let [this ^ListCell this]
                (proxy-super cancelEdit)
                (ui/remove-style! this "editing-cell")
                (.setText this (get-value-string (.getItem this)))
                (.setGraphic this nil)))
            (updateItem [item empty]
              (let [this ^ListCell this]
                (proxy-super updateItem item empty)
                (if empty
                  (do (.setText this nil)
                      (.setGraphic this nil))
                  (do
                    (ui/remove-style! this "editing-cell")
                    (.setText this (get-value-string item))
                    (.setGraphic this nil)))))))))))

(defn- nil->neg1 [index]
  (if (nil? index)
    -1
    index))

(defn- neg1->nil [index]
  (if (= -1 index)
    nil
    index))

(defn- get-selected-index [^ListView list-view]
  (let [ix (-> list-view (.getSelectionModel) (.getSelectedIndex))]
    (neg1->nil ix)))

(defn- select-index [^ListView list-view index]
  (if index
    (-> list-view (.getSelectionModel) (.selectIndices index nil))
    (-> list-view (.getSelectionModel) (.clearSelection))))

(defn- get-focused-index [^ListView list-view]
  (let [ix (-> list-view (.getFocusModel) (.getFocusedIndex))]
    (neg1->nil ix)))

(defn- focus-index [^ListView list-view index]
  (-> list-view (.getFocusModel) (.focus (nil->neg1 index))))

(defn- remove-list-row [list-data row]
  (remove-vec-index list-data row))

(defn- create-fixed-cell-size-list-view ^ListView []
  (let [list-view (ListView.)]
    (.setFixedCellSize list-view 26)
    (.bind (.prefHeightProperty list-view)
           (.add (.multiply (Bindings/size (.getItems list-view))
                            (.getFixedCellSize list-view))
                 52))
  list-view))

(defmethod create-field-control :list [field-info {:keys [set cancel] :as field-ops} ctxt]
  (let [list-view (create-fixed-cell-size-list-view)
        content (atom nil)
        default-row (form/field-default (:element field-info))
        set-row (fn [row val]
                  (swap! content
                         assoc row val)
                  (set (:path field-info) @content))
        on-add-row (fn []
                     (swap! content conj default-row)
                     (set (:path field-info) @content))
        on-remove-row (fn []
                        (swap! content remove-list-row (get-selected-index list-view))
                        (set (:path field-info) @content))
        update-fn (fn [value]
                    (reset! content (if (seq value) value []))
                    (let [old-selected (get-selected-index list-view)
                          old-focus (get-focused-index list-view)]
                      (.setAll (.getItems list-view) ^Collection @content)
                      (select-index list-view old-selected)
                      (focus-index list-view old-focus)))]

    (.setCellFactory list-view (create-list-cell-factory (:element field-info) ctxt))
    (.setOnEditCommit list-view
                      (ui/event-handler event
                                        (let [event ^ListView$EditEvent event]
                                          (set-row (.getIndex event)
                                                   (.getNewValue event)))))
    (.setEditable list-view true)

    (.setPrefWidth list-view (field-width field-info))
    (.setMinWidth list-view Control/USE_PREF_SIZE)

    (set-context-menu! list-view [["Add" on-add-row (constantly default-row)]
                                  ["Remove" on-remove-row #(get-selected-index list-view)]])

    [list-view {:update update-fn}]))

(declare create-section-grid-rows)
(declare ^GridPane create-form-grid)
(declare update-fields)

(defn- wipe-fields [updaters]
  (doseq [[_ updater] updaters]
    (updater nil)))

(defn- build-form-values [field-value-map]
  (reduce-kv (fn [m k v]
               (assoc m [k] v))
             {}
             field-value-map))

(defmethod create-field-control :2panel [field-info field-ops ctxt]
  (let [list-view (doto (create-fixed-cell-size-list-view)
                    (.setMinWidth Control/USE_PREF_SIZE))
        hbox (HBox.)
        content (atom nil)
        nested-field-ops {:set (fn [path val]
                                 (let [selected-index (get-selected-index list-view)]
                                   (when selected-index
                                     (if (contains? field-info :set)
                                       ((:set field-info) (get @content selected-index) path val)
                                       ((:set field-ops) (concat (:path field-info) [selected-index] path) val)))))
                          :clear (when-let [clear (:clear field-ops)]
                                   (fn [path]
                                     (let [selected-index (get-selected-index list-view)]
                                       (clear (concat (:path field-info) [selected-index] path)))))
                          }
        grid-rows (mapcat (fn [section-info] (create-section-grid-rows section-info nested-field-ops ctxt)) (:sections (:panel-form field-info)))
        updaters (into {} (keep :update-ui-fn grid-rows))
        grid (create-form-grid grid-rows)
        panel-key-info (:panel-key field-info)
        internal-select-change (atom false)
        update-list-and-form (fn []
                               (let [panel-keys (map (last (:path panel-key-info)) @content)]
                                 (when (not= (.getItems list-view) panel-keys)
                                   (let [old-selected (get-selected-index list-view)
                                         old-focus (get-focused-index list-view)]
                                     (reset! internal-select-change true)
                                     (.setAll (.getItems list-view) ^Collection panel-keys)
                                     (reset! internal-select-change false)

                                     (select-index list-view old-selected)
                                     (focus-index list-view old-focus))))
                               (if-let [selected-index (get-selected-index list-view)]
                                 (do
                                   (.setDisable grid false)
                                   (let [selected-field-values (@content selected-index)]
                                     (update-fields (ui/user-data grid ::ui-update-fns)
                                                    (build-form-values selected-field-values))))
                                 (do
                                   (.setDisable grid true)
                                   (wipe-fields (ui/user-data grid ::ui-update-fns)))))
        update-fn (fn [value]
                    (reset! content value)
                    (update-list-and-form))
        default-row (form/form-defaults (:panel-form field-info))
        set-panel-key (fn [row val]
                        (swap! content assoc-in
                               (cons row (:path panel-key-info))
                               val)
                        ((:set field-ops) (:path field-info) @content))
        on-add-row (:on-add field-info (fn []
                                         (swap! content conj default-row)
                                         ((:set field-ops) (:path field-info) @content)))
        on-remove-row (fn []
                        (let [idx (get-selected-index list-view)
                              v (get @content idx)]
                          (swap! content remove-table-row idx)
                          (if (contains? field-info :on-remove)
                            ((:on-remove field-info) v)
                            ((:set field-ops) (:path field-info) @content))))]
    (ui/user-data! grid ::ui-update-fns updaters)
    (HBox/setHgrow grid Priority/ALWAYS)

    (ui/children! hbox [list-view grid])
    (.setSpacing hbox 10) ; same as inset of outer VBox

    (.setCellFactory list-view (create-list-cell-factory panel-key-info ctxt))

    (.setOnEditCommit list-view
                      (ui/event-handler event
                                        (let [event ^ListView$EditEvent event]
                                          (set-panel-key (.getIndex event)
                                                         (.getNewValue event)))))


    (.setEditable list-view true)

    (set-context-menu! list-view [["Add" on-add-row (constantly default-row)]
                                  ["Remove" on-remove-row #(get-selected-index list-view)]])

    (ui/observe (.selectedIndexProperty (.getSelectionModel list-view))
                (fn [& _]
                  (when-not @internal-select-change
                    (update-list-and-form))))

    (GridPane/setFillWidth hbox true)

    [hbox {:update update-fn}]))


(defn- create-field-label [name]
  (let [label (Label. name)]
    (.setMinWidth label Control/USE_PREF_SIZE)
    label))

(defn- field-label-valign [field-info]
  (get {:table VPos/TOP :list VPos/TOP :2panel VPos/TOP} (:type field-info) VPos/CENTER))

(defn- create-field-grid-row [field-info {:keys [set clear] :as field-ops} ctxt]
  (let [path (:path field-info)
        label (create-field-label (:label field-info))
        reset-btn (doto (Button. nil (jfx/get-image-view "icons/32/Icons_S_02_Reset.png"))
                    (ui/add-styles! ["clear-button" "button-small"])
                    (ui/on-action! (fn [_] (clear path))))
        [control api] (create-field-control field-info field-ops ctxt)
        label-box (let [box (GridPane.)]
                    (GridPane/setFillWidth label true)
                    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                                        (.setHgrow Priority/ALWAYS))))
                    (.. box getColumnConstraints (add (doto (ColumnConstraints.)
                                                        (.setHgrow Priority/NEVER))))
                    box)
        update-label-box (fn [overridden?]
                           (if overridden?
                             (do
                               (ui/children! label-box [label reset-btn])
                               (GridPane/setConstraints label 0 0)
                               (GridPane/setConstraints reset-btn 1 0))
                             (do
                               (ui/children! label-box [label])
                               (GridPane/setConstraints label 0 0))))
                           
        update-ui-fn (fn [{:keys [value source]}]
                       (let [value (condp = source
                                         :explicit value
                                         :default (form/field-default field-info)
                                         nil)
                             overridden? (and (boolean (:optional field-info)) (= source :explicit))]
                         ((:update api) value)
                         (update-label-box overridden?)))]

    (GridPane/setFillWidth label-box true)
    (GridPane/setValignment label-box (field-label-valign field-info))
    (GridPane/setHalignment label-box HPos/RIGHT)
    {:ctrls [label-box control] :col-spans [1 1] :update-ui-fn [path update-ui-fn]}))

(defn- create-title-label [title]
  (let [label (Label. title)]
    (ui/add-style! label "section-label")
    label))

(defn- create-help-text [help]
  (let [label (Label. help)]
    (ui/add-style! label "help-line")
    label))

(defn- create-section-grid-rows [section-info field-ops ctxt]
  (let [{:keys [help title fields]} section-info]
    (when (> (count fields) 0)
      (concat
       (when title
         [{:ctrls [(create-title-label title)] :col-spans [2]}])
       (when help
         [{:ctrls [(create-help-text help)] :col-spans [2]}])
       (map (fn [field-info] (create-field-grid-row field-info field-ops ctxt)) fields)))))

(defn- update-fields [updaters field-values]
  (doseq [[path val] field-values]
    (when-let [updater (updaters path)]
      (updater {:value val :source :explicit})))
  (let [defaulted-paths (clojure.set/difference (set (keys updaters)) (set (keys field-values)))]
    (doseq [path defaulted-paths]
      ((updaters path) {:source :default}))))

(defn update-form [form form-data]
  (let [updaters (ui/user-data form ::update-ui-fns)]
    (update-fields updaters (:values form-data))
    form))

(defn- add-grid-row [^GridPane grid row row-data]
  (let [{:keys [ctrls col-spans]} row-data]
    (reduce (fn [col [ctrl col-span]]
              (GridPane/setConstraints ctrl col row)
              (GridPane/setColumnSpan ctrl (int col-span))
              (.add (.getChildren grid) ctrl)
              (+ col col-span))
            0
            (map vector ctrls col-spans))))

(defn- add-grid-rows [grid grid-rows]
  (doall (map-indexed (partial add-grid-row grid) grid-rows)))

(defn- make-base-field-ops [form-ops]
  {:set (fn [path value] (form/set-value! form-ops path value))
   :clear (when (form/can-clear? form-ops)
            (fn [path] (form/clear-value! form-ops path)))
   })

(defn- create-form-grid [grid-rows]
  (let [grid (doto (GridPane.)
               (.setHgap grid-hgap)
               (.setVgap grid-vgap))
        cc1  (doto (ColumnConstraints.) (.setHgrow Priority/NEVER))
        cc2  (doto (ColumnConstraints.) (.setHgrow Priority/ALWAYS) (.setFillWidth false))]
    (.. grid getColumnConstraints (add cc1))
    (.. grid getColumnConstraints (add cc2))
    (ui/add-style! grid "form")
    (add-grid-rows grid grid-rows)
    grid))

(defn- create-form [form-data ctxt]
  (let [base-field-ops (make-base-field-ops (:form-ops form-data))
        grid-rows (mapcat (fn [section-info] (create-section-grid-rows section-info base-field-ops ctxt)) (:sections form-data))
        updaters (into {} (keep :update-ui-fn grid-rows))
        grid (create-form-grid grid-rows)
        vbox (doto (VBox. (double 10.0))
               (.setPadding (Insets. 10 10 10 10))
               (.setFillWidth true)
               (ui/children! [grid]))
        scroll-pane (doto (ScrollPane. vbox)
                      (ui/fill-control)
                      (.setFitToWidth true))]
    (ui/user-data! scroll-pane ::update-ui-fns updaters)
    (ui/user-data! scroll-pane ::form-data form-data)
    scroll-pane))

(defn- same-form-structure [form-data1 form-data2]
  (= (select-keys form-data1 [:form-ops :sections])
     (select-keys form-data2 [:form-ops :sections])))

(g/defnk produce-update-form [parent-view _node-id workspace project form-data]
  (let [prev-form (g/node-value _node-id :prev-form)
        prev-form-data (and prev-form (ui/user-data prev-form ::form-data))]
    (if (and prev-form (same-form-structure prev-form-data form-data))
      (update-form prev-form form-data)
      (let [form (create-form form-data {:workspace workspace :project project})]
        (update-form form form-data)
        (ui/children! parent-view [form])
        (g/set-property! _node-id :prev-form form)
        form))))

(g/defnode FormView
  (property parent-view Parent)
  (property workspace g/Any)
  (property project g/Any)
  (property prev-form ScrollPane)
  (input form-data g/Any :substitute {})
  (output form ScrollPane :cached produce-update-form))

(defn- do-make-form-view [graph ^Parent parent resource-node opts]
  (let [workspace (:workspace opts)
        project (:project opts)
        view-id (g/make-node! graph FormView :parent-view parent :workspace workspace :project project)
        repainter (ui/->timer 10 "refresh-form-view" (fn [_ dt] (g/node-value view-id :form)))]
    (g/transact
      (concat
        (g/connect resource-node :form-data view-id :form-data)))
    (ui/timer-start! repainter)
    (ui/timer-stop-on-closed! ^Tab (:tab opts) repainter)
    (ui/timer-stop-on-closed! (ui/parent->stage parent) repainter)
    view-id))

(defn- make-form-view [graph ^Parent parent resource-node opts]
  (do-make-form-view graph parent resource-node opts))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :form-view
                                :label "Form"
                                :make-view-fn make-form-view))

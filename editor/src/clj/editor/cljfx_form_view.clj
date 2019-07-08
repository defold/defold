(ns editor.cljfx-form-view
  (:require [cljfx.api :as fx]
            [cljfx.ext.list-view :as fx.ext.list-view]
            [cljfx.ext.table-view :as fx.ext.table-view]
            [cljfx.fx.anchor-pane :as fx.anchor-pane]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.form :as form]
            [editor.fxui :as fxui]
            [editor.jfx :as jfx]
            [editor.handler :as handler]
            [editor.resource :as resource]
            [editor.settings :as settings]
            [editor.ui :as ui]
            [editor.url :as url]
            [editor.view :as view]
            [editor.workspace :as workspace])
  (:import [javafx.event Event]
           [javafx.scene Node]
           [javafx.scene.control Cell ComboBox ListView$EditEvent ScrollPane TableColumn$CellEditEvent TableView]
           [javafx.scene.input KeyCode KeyEvent]
           [javafx.util StringConverter]))

(set! *warn-on-reflection* true)

(def ^:private cell-height 27)

(g/defnk produce-form-view [renderer form-data ui-state]
  (renderer {:form-data form-data
             :ui-state ui-state})
  nil)

(g/defnode CljfxFormView
  (inherits view/WorkbenchView)
  (input form-data g/Any :substitute {})
  (property ui-state g/Any (default {:filter-term ""}))
  (property renderer g/Any)
  (output form-view g/Any :cached produce-form-view))

(defmulti handle-event :event-type)

(defmethod handle-event :default [e]
  (prn :handle
       (:event-type e)
       (dissoc e :event-type :ui-state :form-data :workspace :project :parent)))

(defmethod handle-event :clear [{:keys [path]}]
  {:clear path})

(defmethod handle-event :set [{:keys [path fx/event]}]
  {:set [path event]})

(defmethod handle-event :filter-text-changed [{:keys [ui-state fx/event]}]
  {:set-ui-state (assoc ui-state :filter-term event)})

(defmethod handle-event :filter-key-pressed [{:keys [^KeyEvent fx/event ui-state]}]
  (when (= KeyCode/ESCAPE (.getCode event))
    {:set-ui-state (assoc ui-state :filter-term "")}))

(defmethod handle-event :jump-to [{:keys [section ^Node parent]}]
  (let [^ScrollPane scroll-pane (.lookup parent "#scroll-pane")
        ^Node node (.lookup parent (str "#" section))
        content-height (-> scroll-pane .getContent .getBoundsInLocal .getHeight)
        viewport-height (-> scroll-pane .getViewportBounds .getHeight)]
    (when (> content-height viewport-height)
      (.setVvalue scroll-pane (/ (- (.getMinY (.getBoundsInParent node))
                                    24) ;; form padding
                                 (- content-height viewport-height))))))

(def with-anchor-pane-props
  (fx/make-ext-with-props fx.anchor-pane/props))

(def uri-string-converter
  (proxy [StringConverter] []
    (toString
      ([] "uri-string-converter")
      ([v] (str v)))
    (fromString [v]
      (url/try-parse v))))

(def number-converter
  (proxy [StringConverter] []
    (toString
      ([] "number-string-converter")
      ([v] (field-expression/format-number v)))
    (fromString [v]
      (field-expression/to-double v))))

(defn- make-resource-string-converter [workspace]
  (proxy [StringConverter] []
    (toString
      ([] "resource-string-converter")
      ([v] (resource/resource->proj-path v)))
    (fromString [v]
      (some->> (when-not (string/blank? v) v)
               (workspace/to-absolute-path)
               (workspace/resolve-workspace-resource workspace)))))

(defn- contains-ignore-case? [^String str ^String sub]
  (let [sub-length (.length sub)]
    (if (zero? sub-length)
      true
      (let [str-length (.length str)]
        (loop [i 0]
          (cond
            (= i str-length) false
            (.regionMatches str true i sub 0 sub-length) true
            :else (recur (inc i))))))))

(defn- text-field [props]
  (assoc props :fx/type :text-field
               :style-class ["text-field" "cljfx-form-text-field"]))

(defn- add-image-fit-size [{:keys [fit-size] :as props}]
  (-> props
      (dissoc :fit-size)
      (update :graphic assoc :fit-width fit-size :fit-height fit-size)))

(defn- add-image [props]
  (cond-> (-> props
              (dissoc :image)
              (assoc :graphic {:fx/type :image-view
                               :image (jfx/get-image (:image props))}))

          (contains? props :fit-size)
          add-image-fit-size))

(defn- icon-button [props]
  (cond-> (assoc props :fx/type :button
                       :style-class ["button" "cljfx-form-icon-button"])

          (contains? props :image)
          add-image))

(defmulti form-input-view :type)

(defmethod form-input-view :default [{:keys [value type] :as field}]
  {:fx/type :label
   :wrap-text true
   :text (str type " " (if (nil? value) "***NIL***" value) " " field)})

;; region string input

(defn- string-input [{:keys [value on-value-changed]}]
  {:fx/type text-field
   :text-formatter {:fx/type :text-formatter
                    :value-converter :default
                    :value value
                    :on-value-changed on-value-changed}})

(defmethod form-input-view :string [{:keys [value path]}]
  {:fx/type string-input
   :value value
   :on-value-changed {:event-type :set
                      :path path}})

;; endregion

;; region boolean input

(defn- boolean-input [{:keys [value on-value-changed]}]
  {:fx/type :check-box
   :style-class ["check-box" "cljfx-form-check-box"]
   :selected value
   :on-selected-changed on-value-changed})

(defmethod form-input-view :boolean [{:keys [value path]}]
  {:fx/type boolean-input
   :value value
   :on-value-changed {:event-type :set
                      :path path}})

;; endregion

;; region integer input

(defn- integer-input [{:keys [value on-value-changed]}]
  {:fx/type text-field
   :alignment :center-right
   :max-width 80
   :text-formatter {:fx/type :text-formatter
                    :value-converter :integer
                    :value (int value)
                    :on-value-changed on-value-changed}})

(defmethod form-input-view :integer [{:keys [value path]}]
  {:fx/type integer-input
   :value value
   :on-value-changed {:event-type :set
                      :path path}})

;; endregion

;; region number input

(defn- number-input [{:keys [value on-value-changed]}]
  {:fx/type text-field
   :alignment :center-right
   :max-width 80
   :text-formatter {:fx/type :text-formatter
                    :value-converter number-converter
                    :value value
                    :on-value-changed on-value-changed}})

(defmethod form-input-view :number [{:keys [value path]}]
  {:fx/type number-input
   :value value
   :on-value-changed {:event-type :set
                      :path path}})

;; endregion

;; region vec4 input

(defmethod handle-event :route-vec4-change [{:keys [value index to fx/event]}]
  {:dispatch (assoc to :fx/event (assoc value index event))})

(defn- vec4-input [{:keys [value on-value-changed]}]
  (let [labels ["X" "Y" "Z" "W"]]
    {:fx/type :h-box
     :padding {:left 5}
     :spacing 5
     :children (into []
                     (map-indexed
                       (fn [i n]
                         {:fx/type :h-box
                          :alignment :center
                          :spacing 5
                          :children [{:fx/type :label
                                      :min-width :use-pref-size
                                      :text (get labels i)}
                                     {:fx/type number-input
                                      :h-box/hgrow :always
                                      :value n
                                      :on-value-changed {:event-type :route-vec4-change
                                                         :to on-value-changed
                                                         :value value
                                                         :index i}}]}))
                     value)}))

(defmethod form-input-view :vec4 [{:keys [value path]}]
  {:fx/type vec4-input
   :value value
   :on-value-changed {:event-type :set
                      :path path}})

;; endregion

;; region choicebox input

(defn- choicebox-input [{:keys [value on-value-changed options from-string to-string]
                         :or {to-string str}}]
  (let [value->label (into {} options)
        label->value (set/map-invert value->label)]
    {:fx/type :combo-box
     :style-class ["combo-box" "combo-box-base" "cljfx-form-combo-box"]
     :value value
     :on-value-changed on-value-changed
     :converter (proxy [StringConverter] []
                  (toString
                    ([] "string-converter")
                    ([value]
                     (get value->label value (to-string value))))
                  (fromString [s]
                    (get label->value s (and from-string (from-string s)))))
     :editable (some? from-string)
     :button-cell (fn [x]
                    {:text (value->label x)})
     :cell-factory (fn [x]
                     {:text (value->label x)})
     :items (sort (mapv first options))}))

(defmethod form-input-view :choicebox [{:keys [path] :as field}]
  (assoc field :fx/type choicebox-input
               :on-value-changed {:event-type :set
                                  :path path}))

;; endregion

;; region list view input

(defmethod handle-event :route-list-item-edit [{:keys [^ListView$EditEvent fx/event to]}]
  (let [new-element (.getNewValue event)]
    (when (some? new-element)
      {:dispatch (assoc to :fx/event {:index (.getIndex event) :item new-element})})))

(defmethod handle-event :route-added-list-element [{:keys [value
                                                           value-to
                                                           state-path
                                                           ui-state
                                                           element]}]
  [[:dispatch (assoc value-to :fx/event [element])]
   [:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) [(count value)])]])

(defmethod handle-event :route-added-list-resources [{:keys [workspace
                                                             project
                                                             filter
                                                             value
                                                             value-to
                                                             state-path
                                                             ui-state]}]
  (let [resources (dialogs/make-resource-dialog workspace project {:ext filter
                                                                   :selection :multiple})]
    (when-not (empty? resources)
      (let [value-count (count value)
            added-count (count resources)
            indices (vec (range value-count
                                (+ value-count added-count)))]
        [[:dispatch (assoc value-to :fx/event resources)]
         [:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) indices)]]))))

(defmethod handle-event :remove-list-selection [{:keys [value-to
                                                        state-path
                                                        ui-state]}]
  (let [indices-path (conj state-path :selected-indices)]
    [[:dispatch (assoc value-to :fx/event (set (get-in ui-state indices-path)))]
     [:set-ui-state (assoc-in ui-state indices-path [])]]))

(defmethod handle-event :route-list-select [{:keys [state-path fx/event ui-state]}]
  {:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) event)})

(defn- list-input [{:keys [;; state
                           state
                           state-path
                           ;; value
                           value
                           on-edited ;; fx/event is {:index int :item item}
                           on-added ;; fx/event is [item ...]
                           on-removed ;; fx/event is #{index ...}
                           ;; field
                           element
                           resource-string-converter]
                    :or {state {:selected-indices []}
                         value []}}]
  (let [{:keys [selected-indices]} state
        disable-add (not (form/has-default? element))
        disable-remove (empty? selected-indices)
        add-event (if (= :resource (:type element))
                    {:event-type :route-added-list-resources
                     :value value
                     :value-to on-added
                     :state-path state-path
                     :filter (:filter element)}
                    {:event-type :route-added-list-element
                     :value value
                     :value-to on-added
                     :state-path state-path
                     :element (form/field-default element)})
        remove-event {:event-type :remove-list-selection
                      :value-to on-removed
                      :state-path state-path}]
    {:fx/type :v-box
     :spacing 4
     :children [{:fx/type fxui/ext-with-advance-events
                 :desc {:fx/type fx.ext.list-view/with-selection-props
                        :props {:selection-mode :multiple
                                :selected-indices selected-indices
                                :on-selected-indices-changed {:event-type :route-list-select
                                                              :state-path state-path}}
                        :desc {:fx/type :list-view
                               :style-class ["list-view" "cljfx-form-list-view"]
                               :items (vec value)
                               :editable true
                               :on-edit-commit {:event-type :route-list-item-edit
                                                :to on-edited}
                               :pref-height (+ 2                   ;; top and bottom insets
                                               1                   ;; bottom padding
                                               9                   ;; horizontal progress bar height
                                               (* cell-height
                                                  (max (count value) 1)))
                               :fixed-cell-size cell-height
                               :cell-factory (fn [v]
                                               (case (:type element)
                                                 :url
                                                 {:text (str v)
                                                  :converter uri-string-converter}

                                                 :resource
                                                 {:text (resource/resource->proj-path v)
                                                  :converter resource-string-converter}

                                                 :string
                                                 {:text v
                                                  :converter :default}

                                                 {:text (str v)
                                                  :converter :default}))

                               :context-menu {:fx/type :context-menu
                                              :items [{:fx/type :menu-item
                                                       :text "Add"
                                                       :disable disable-add
                                                       :on-action add-event}
                                                      {:fx/type :menu-item
                                                       :text "Remove"
                                                       :disable disable-remove
                                                       :on-action remove-event}]}}}}
                {:fx/type :h-box
                 :spacing 4
                 :children [{:fx/type icon-button
                             :disable disable-add
                             :on-action add-event
                             :image "icons/32/Icons_M_07_plus.png"
                             :fit-size 16}
                            {:fx/type icon-button
                             :disable disable-remove
                             :on-action remove-event
                             :image "icons/32/Icons_M_11_minus.png"
                             :fit-size 16}]}]}))

(defmethod handle-event :add-list-items [{:keys [value path fx/event]}]
  {:set [path (into value event)]})

(defmethod handle-event :remove-list-items [{:keys [value path fx/event]}]
  {:set [path (into []
                    (keep-indexed
                      (fn [i x]
                        (when-not (contains? event i)
                          x)))
                    value)]})

(defmethod handle-event :edit-list-item [{:keys [value path fx/event]}]
  (let [{:keys [index item]} event]
    {:set [path (assoc value index item)]}))

(defmethod form-input-view :list [{:keys [path value]
                                   :or {value []}
                                   :as field}]
  (assoc field :fx/type list-input
               :state-path path
               :on-edited {:event-type :edit-list-item
                           :value value
                           :path path}
               :on-added {:event-type :add-list-items
                          :value value
                          :path path}
               :on-removed {:event-type :remove-list-items
                            :value value
                            :path path}))

;; endregion

;; region resource input

(defmethod handle-event :open-resource [{:keys [^Event fx/event value]}]
  {:open-resource [(.getTarget event) value]})

(defmethod handle-event :route-selected-resource [{:keys [workspace project filter to]}]
  (when-let [resource (first (dialogs/make-resource-dialog workspace project {:ext filter}))]
    {:dispatch (assoc to :fx/event resource)}))

(defn- resource-input [{:keys [value on-value-changed filter resource-string-converter]}]
  {:fx/type :h-box
   :spacing 4
   :children [{:fx/type text-field
               :h-box/hgrow :always
               :text-formatter {:fx/type :text-formatter
                                :value-converter resource-string-converter
                                :value value
                                :on-value-changed on-value-changed}}
              {:fx/type icon-button
               :disable (not (and (resource/openable-resource? value)
                                  (resource/exists? value)))
               :image "icons/32/Icons_S_14_linkarrow.png"
               :fit-size 22
               :on-action {:event-type :open-resource
                           :value value}}
              {:fx/type icon-button
               :text "\u2026"
               :on-action {:event-type :route-selected-resource
                           :to on-value-changed
                           :filter filter}}]})

(defmethod form-input-view :resource [{:keys [path value filter resource-string-converter]}]
  {:fx/type resource-input
   :value value
   :on-value-changed {:event-type :set :path path}
   :filter filter
   :resource-string-converter resource-string-converter})

;; endregion

;; region file input

(defmethod handle-event :route-selected-file [{:keys [to filter ^Event fx/event title]}]
  (when-let [file (dialogs/make-file-dialog (or title "Select File")
                                            filter
                                            nil
                                            (.getWindow (.getScene ^Node (.getSource event))))]
    {:dispatch (assoc to :fx/event (.getAbsolutePath file))}))

(defn- file-input [{:keys [value on-value-changed filter title]}]
  {:fx/type :h-box
   :spacing 4
   :children [{:fx/type text-field
               :h-box/hgrow :always
               :text-formatter {:fx/type :text-formatter
                                :value-converter :default
                                :value value
                                :on-value-changed on-value-changed}}
              {:fx/type icon-button
               :text "\u2026"
               :on-action {:event-type :route-selected-file
                           :to on-value-changed
                           :filter filter
                           :title title}}]})

(defmethod form-input-view :file [{:keys [path value filter title]}]
  {:fx/type file-input
   :value value
   :on-value-changed {:event-type :set :path path}
   :filter filter
   :title title})

;; endregion

;; region directory input

(defmethod handle-event :route-selected-directory [{:keys [to title]}]
  (when-let [file (ui/choose-directory (or title "Select Directory") nil)]
    {:dispatch (assoc to :fx/event file)}))

(defn- directory-input [{:keys [value on-value-changed title]}]
  {:fx/type :h-box
   :spacing 4
   :children [{:fx/type text-field
               :h-box/hgrow :always
               :text-formatter {:fx/type :text-formatter
                                :value-converter :default
                                :value value
                                :on-value-changed on-value-changed}}
              {:fx/type icon-button
               :text "\u2026"
               :on-action {:event-type :route-selected-directory
                           :to on-value-changed
                           :title title}}]})

(defmethod form-input-view :directory [{:keys [path value title]}]
  {:fx/type directory-input
   :value value
   :on-value-changed {:event-type :set :path path}
   :title title})

;; endregion

;; region table input

(defmethod handle-event :on-table-edit-start [{:keys [^TableColumn$CellEditEvent fx/event
                                                      column-path
                                                      ui-state
                                                      state-path]}]
  (let [index (.getRow (.getTablePosition event))]
    {:set-ui-state (update-in ui-state
                              (conj state-path :edit)
                              (fn [edit]
                                (if (and (= (:index edit) index)
                                         (= (:path edit) column-path))
                                  edit
                                  {:index index
                                   :path column-path
                                   :table (.getTableView event)})))}))

(defn- value-with-edit [value edit]
  (let [edit-path (into [(:index edit)] (:path edit))]
    (assoc-in value edit-path (:value edit))))

(defmethod handle-event :on-table-edit-cancel [{:keys [value
                                                       value-to
                                                       ui-state
                                                       state-path]}]
  (let [edit (get-in ui-state (conj state-path :edit))]
    (cond-> [[:set-ui-state (update-in ui-state state-path dissoc :edit)]]

            (some? (:value edit))
            (conj [:dispatch (assoc value-to :fx/event (value-with-edit value edit))]))))

(defmethod handle-event :commit-table-edit [{:keys [fx/event
                                                    edit-path
                                                    ui-state
                                                    state-path
                                                    value
                                                    value-to]}]
  (.edit ^TableView (get-in ui-state (conj state-path :edit :table)) -1 nil)
  {:dispatch (assoc value-to :fx/event (assoc-in value edit-path event))})

(defmethod handle-event :keep-table-edit [{:keys [ui-state state-path fx/event]}]
  (let [state (get-in ui-state state-path)]
    (when (contains? state :edit)
      {:set-ui-state (assoc-in ui-state (conj state-path :edit :value) event)})))

(defmethod handle-event :cancel-table-edit-on-escape [{:keys [^KeyEvent fx/event
                                                              ui-state
                                                              state-path]}]
  (when (= KeyCode/ESCAPE (.getCode event))
    (when-let [^Cell cell (ui/closest-node-of-type Cell (.getTarget event))]
      [[:set-ui-state (update-in ui-state state-path dissoc :edit)]
       [:cancel-edit cell]])))

(defmethod handle-event :commit-kept-table-edit-on-enter [{:keys [^KeyEvent fx/event
                                                                  edit-path
                                                                  ui-state
                                                                  state-path
                                                                  value
                                                                  value-to]}]
  (when (= KeyCode/ENTER (.getCode event))
    (when-let [edit (get-in ui-state (conj state-path :edit))]
      (when (some? (:value edit))
        {:dispatch {:event-type :commit-table-edit
                    :edit-path edit-path
                    :state-path state-path
                    :value value
                    :value-to value-to
                    :fx/event (:value edit)}}))))

(defmethod handle-event :route-add-table-element [{:keys [ui-state
                                                          state-path
                                                          value
                                                          value-to
                                                          element]}]
  (let [new-value (into value [element])]
    [[:dispatch (assoc value-to :fx/event new-value)]
     [:set-ui-state (assoc-in ui-state
                              (conj state-path :selected-indices)
                              [(dec (count new-value))])]]))

(defmethod handle-event :remove-table-selection [{:keys [value value-to ui-state state-path]}]
  (let [indices-path (conj state-path :selected-indices)
        selected-indices (set (get-in ui-state indices-path))
        new-value (into []
                        (keep-indexed
                          (fn [i x]
                            (when-not (contains? selected-indices i)
                              x)))
                        value)]
    [[:dispatch (assoc value-to :fx/event new-value)]
     [:set-ui-state (assoc-in ui-state indices-path [])]]))

(defmethod handle-event :table-select [{:keys [ui-state state-path fx/event]}]
  {:set-ui-state (assoc-in ui-state (conj state-path :selected-indices) event)})

(def ^:private ext-with-key-pressed-props
  (fx/make-ext-with-props
    {:on-filter-key-pressed (fxui/make-event-filter-prop KeyEvent/KEY_PRESSED)
     :on-key-pressed (fxui/make-event-handler-prop KeyEvent/KEY_PRESSED)}))

(defn- edited-cell-view [column
                         {:keys [value on-value-changed state-path]
                          :or {value []}}
                         i
                         item]
  (let [wrap-input (fn [desc]
                     {:fx/type ext-with-key-pressed-props
                      :props {:on-filter-key-pressed {:event-type :cancel-table-edit-on-escape
                                                      :state-path state-path}}
                      :desc desc})
        edit-path (into [i] (:path column))]
    (case (:type column)
      :choicebox
      {:fx/type fx/ext-on-instance-lifecycle
       :on-created #(.show ^ComboBox %)
       :desc (wrap-input
               (assoc column :fx/type choicebox-input
                             :value item
                             :on-value-changed {:event-type :commit-table-edit
                                                :edit-path edit-path
                                                :state-path state-path
                                                :value value
                                                :value-to on-value-changed}))}

      :vec4
      {:fx/type ext-with-key-pressed-props
       :props {:on-key-pressed {:event-type :commit-kept-table-edit-on-enter
                                :edit-path edit-path
                                :state-path state-path
                                :value value
                                :value-to on-value-changed}}
       :desc {:fx/type fx/ext-on-instance-lifecycle
              :on-created #(fxui/focus-when-on-scene! (.lookup ^Node % "TextField"))
              :desc (wrap-input
                      (assoc column :fx/type vec4-input
                                    :value item
                                    :on-value-changed {:event-type :keep-table-edit
                                                       :state-path state-path}))}}

      :string
      {:fx/type fxui/ext-focused-by-default
       :desc (wrap-input (assoc column :fx/type string-input
                                       :value item
                                       :on-value-changed {:event-type :commit-table-edit
                                                          :edit-path edit-path
                                                          :state-path state-path
                                                          :value value
                                                          :value-to on-value-changed}))}

      {:fx/type :label
       :wrap-text true
       :text (str (:type column) " " (pr-str value))})))

(defn- table-cell-value-factory [path [i x]]
  [i (get-in x path)])

(defn- table-cell-factory [{:keys [path type] :as column} edit [i x]]
  (let [edited (and (= i (:index edit))
                    (= path (:path edit)))]
    (case [type edited]
      [:choicebox false]
      {:text (get (into {} (:options column)) x)}

      [:choicebox true]
      {:style {:-fx-padding -2}
       :graphic {:fx/type fx/ext-get-ref :ref ::edit}}

      [:vec4 false]
      {:text (->> x
                  (mapv field-expression/format-number)
                  (string/join "  "))}

      [:vec4 true]
      {:style {:-fx-padding "-2 0 0 0"}
       :graphic {:fx/type fx/ext-get-ref :ref ::edit}}

      [:string false]
      {:text x}

      [:string true]
      {:style {:-fx-padding -1}
       :graphic {:fx/type fx/ext-get-ref :ref ::edit}}

      {:text (str type ": " x)})))

(defn- table-column [{:keys [path label type] :as column}
                     {:keys [state state-path value on-value-changed]
                      :or {value []}}]
  (let [{:keys [edit]} state]
    {:fx/type :table-column
     :reorderable false
     :sortable false
     :min-width (cond
                  (and (= :vec4 type)
                       (= path (:path edit)))
                  235

                  :else
                  80)
     :on-edit-start {:event-type :on-table-edit-start
                     :state-path state-path
                     :column-path path}
     :on-edit-cancel {:event-type :on-table-edit-cancel
                      :value-to on-value-changed
                      :state-path state-path}
     :text label
     :cell-value-factory (fxui/partial table-cell-value-factory path)
     :cell-factory (fxui/partial table-cell-factory column (dissoc edit :value))}))

(defn- table-input [{:keys [value
                            on-value-changed
                            columns
                            state
                            state-path]
                     :or {value []}
                     :as field}]
  (let [{:keys [selected-indices edit]} state
        disable-remove (empty? selected-indices)
        default-row (form/table-row-defaults field)
        disable-add (nil? default-row)
        on-added {:event-type :route-add-table-element
                  :value value
                  :value-to on-value-changed
                  :state-path state-path
                  :element default-row}
        on-removed {:event-type :remove-table-selection
                    :value value
                    :value-to on-value-changed
                    :state-path state-path}]
    {:fx/type fx/ext-let-refs
     :refs (when edit
             (let [edited-column (some #(when (= (:path edit) (:path %))
                                          %)
                                       columns)
                   edited-value (or (:value edit)
                                    (get-in value (into [(:index edit)] (:path edit))))]
               {::edit (edited-cell-view edited-column field (:index edit) edited-value)}))
     :desc {:fx/type :v-box
            :spacing 4
            :children [{:fx/type :stack-pane
                        :style-class "cljfx-table-view-wrapper"
                        :children
                        [{:fx/type fxui/ext-with-advance-events
                          :desc {:fx/type fx.ext.table-view/with-selection-props
                                 :props {:selection-mode :multiple
                                         :selected-indices (vec selected-indices)
                                         :on-selected-indices-changed {:event-type :table-select
                                                                       :state-path state-path}}
                                 :desc {:fx/type :table-view
                                        :style-class ["table-view" "cljfx-table-view"]
                                        :editable true
                                        :fixed-cell-size cell-height
                                        :pref-height (+ cell-height ;; header
                                                        2   ;; insets
                                                        9   ;; bottom scrollbar
                                                        (* cell-height
                                                           (max 1 (count value))))
                                        :columns (mapv #(table-column % field)
                                                       columns)
                                        :items (into [] (map-indexed vector) value)
                                        :context-menu {:fx/type :context-menu
                                                       :items [{:fx/type :menu-item
                                                                :text "Add"
                                                                :disable disable-add
                                                                :on-action on-added}
                                                               {:fx/type :menu-item
                                                                :text "Remove"
                                                                :disable disable-remove
                                                                :on-action on-removed}]}}}}]}
                       {:fx/type :h-box
                        :spacing 4
                        :children [{:fx/type icon-button
                                    :disable disable-add
                                    :on-action on-added
                                    :image "icons/32/Icons_M_07_plus.png"
                                    :fit-size 16}
                                   {:fx/type icon-button
                                    :disable disable-remove
                                    :on-action on-removed
                                    :image "icons/32/Icons_M_11_minus.png"
                                    :fit-size 16}]}]}}))

(defmethod form-input-view :table [{:keys [path]
                                    :as field}]
  (assoc field :fx/type table-input
               :on-value-changed {:event-type :set
                                  :path path}
               :state-path path))

;; endregion

(defn- field-view [field]
  (form-input-view field))

(defn- make-row [values ui-state resource-string-converter row field]
  (let [{:keys [path label help visible]} field
        value (get values path ::no-value)
        state (get-in ui-state path ::no-value)
        error (settings/get-setting-error
                (when-not (= value ::no-value) value)
                field
                :build-targets)]
    (cond-> []
            :always
            (conj (cond->
                    {:fx/type :label
                     :grid-pane/row row
                     :grid-pane/column 0
                     :grid-pane/valignment :top
                     :grid-pane/margin {:top 5}
                     :visible visible
                     :managed visible
                     :alignment :top-left
                     :text label}
                    (not-empty help)
                    (assoc :tooltip {:fx/type :tooltip
                                     :text help})))

            (and (form/optional-field? field)
                 (not= value ::no-value))
            (conj {:fx/type icon-button
                   :grid-pane/row row
                   :grid-pane/column 1
                   :grid-pane/valignment :top
                   :grid-pane/halignment :right
                   :visible visible
                   :managed visible
                   :image "icons/32/Icons_S_02_Reset.png"
                   :on-action {:event-type :clear
                               :path path}})

            :always
            (conj {:fx/type :v-box
                   :style-class (case (:severity error)
                                  :fatal ["cljfx-form-error"]
                                  :warning ["cljfx-form-warning"]
                                  [])
                   :grid-pane/row row
                   :grid-pane/column 2
                   :visible visible
                   :managed visible
                   :min-height cell-height
                   :alignment :center-left
                   :children [(cond-> field
                                      :always
                                      (assoc :fx/type field-view
                                             :resource-string-converter resource-string-converter
                                             :value (if (= ::no-value value)
                                                      (form/field-default field)
                                                      value))

                                      (not= ::no-value state)
                                      (assoc :state state))]}))))

(defn- section-id [title]
  (string/replace title " " "-"))

(defn- section-view [{:keys [title help fields values ui-state resource-string-converter visible]}]
  {:fx/type :v-box
   :id (section-id title)
   :visible visible
   :managed visible
   :children (cond-> []

                     :always
                     (conj {:fx/type :label
                            :style-class ["label" "cljfx-form-title"]
                            :text title})

                     help
                     (conj {:fx/type :label
                            :text help})

                     :always
                     (conj {:fx/type :grid-pane
                            :style-class "cljfx-form-fields"
                            :column-constraints [{:fx/type :column-constraints
                                                  :min-width 150
                                                  :max-width 150}
                                                 {:fx/type :column-constraints
                                                  :min-width cell-height
                                                  :max-width cell-height}
                                                 {:fx/type :column-constraints
                                                  :hgrow :always
                                                  :min-width 200
                                                  :max-width 400}]
                            :children (first
                                        (reduce
                                          (fn [[acc row] field]
                                            [(into acc (make-row values
                                                                 ui-state
                                                                 resource-string-converter
                                                                 row
                                                                 field))
                                             (if (:visible field) (inc row) row)])
                                          [[] 0]
                                          fields))}))})

;; region filtering

(defmulti filterable-strings :type)

(defmethod filterable-strings :default [{:keys [value]}]
  (when (some? value)
    [(str value)]))

(defmethod filterable-strings :boolean [_]
  [])

(defmethod filterable-strings :list [{:keys [value] :as field}]
  (eduction
    (map #(assoc (:element field) :value %))
    (mapcat filterable-strings)
    value))

(defmethod filterable-strings :resource [{:keys [value]}]
  [(resource/resource->proj-path value)])

(defmethod filterable-strings :choicebox [{:keys [value] :as field}]
  (when (some? value)
    [((:to-string field str) value)]))

(defn- set-field-visibility [field values filter-term section-visible]
  (let [value (get values (:path field) ::no-value)
        visible (and (or section-visible
                         (contains-ignore-case? (:label field) filter-term)
                         (boolean (some #(contains-ignore-case? % filter-term)
                                        (filterable-strings
                                          (assoc field :value (if (= value ::no-value)
                                                                (form/field-default field)
                                                                value))))))
                     (or (not (:deprecated field))
                         (some? (settings/get-setting-error
                                  (if (= value ::no-value) nil value)
                                  field
                                  :build-targets))))]
    (assoc field :visible visible)))

(defn- set-section-visibility [{:keys [title help fields] :as section} values filter-term]
  (let [visible (or (contains-ignore-case? title filter-term)
                    (and (some? help)
                         (contains-ignore-case? help filter-term)))
        fields (into []
                     (comp
                       (remove :hidden?)
                       (map #(set-field-visibility % values filter-term visible)))
                     fields)]
    (assoc section :visible (or visible (boolean (some :visible fields)))
                   :fields fields)))

(defn- filter-text-field [{:keys [text]}]
  {:fx/type :text-field
   :id "filter-text-field"
   :style-class ["text-field" "filter-text-field"]
   :prompt-text "Filter"
   :text text
   :on-key-pressed {:event-type :filter-key-pressed}
   :on-text-changed {:event-type :filter-text-changed}})

;; endregion

(defn- make-section-views [sections values ui-state resource-string-converter]
  (first
    (reduce
      (fn
        ([[acc seen-visible] section]
         (let [section-view (assoc section
                              :fx/type section-view
                              :values values
                              :ui-state ui-state
                              :resource-string-converter resource-string-converter)
               visible (:visible section)]
           (if (empty? acc)
             [(conj acc section-view) visible]
             [(into acc [{:fx/type :separator
                          :style-class "cljfx-form-separator"
                          :visible (and seen-visible visible)
                          :managed (and seen-visible visible)}
                         section-view])
              (or seen-visible visible)]))))
      [[] false]
      sections)))

(defn- jump-to-button [{:keys [visible-titles]}]
  {:fx/type :menu-button
   :text "Jump to..."
   :style-class "jump-to-menu-button"
   :disable (empty? visible-titles)
   :items (->> visible-titles
               (sort #(.compareToIgnoreCase ^String %1 %2))
               (mapv (fn [title]
                       {:fx/type :menu-item
                        :on-action {:event-type :jump-to
                                    :section (section-id title)}
                        :text title})))})

(defn- form-view [{:keys [parent form-data ui-state resource-string-converter]}]
  (let [{:keys [sections values]} form-data
        filter-term (:filter-term ui-state)
        sections-with-visibility (mapv #(set-section-visibility % values filter-term)
                                       sections)
        visible-titles (into []
                             (comp
                               (filter :visible)
                               (map :title))
                             sections-with-visibility)
        section-views (make-section-views sections-with-visibility
                                          values
                                          ui-state
                                          resource-string-converter)]
    {:fx/type with-anchor-pane-props
     :desc {:fx/type fxui/ext-value
            :value parent}
     :props {:children (cond-> []
                               (:navigation form-data true)
                               (conj {:fx/type :h-box
                                      :style-class "cljfx-form-floating-area"
                                      :anchor-pane/top 24
                                      :anchor-pane/right 24
                                      :view-order 0
                                      :children [{:fx/type filter-text-field
                                                  :text filter-term}
                                                 {:fx/type jump-to-button
                                                  :visible-titles visible-titles}]})

                               :always
                               (conj {:fx/type fx/ext-on-instance-lifecycle
                                      :anchor-pane/top 0
                                      :anchor-pane/right 0
                                      :anchor-pane/bottom 0
                                      :anchor-pane/left 0
                                      :on-created #(ui/context! % :form {:root parent} nil)
                                      :desc {:fx/type :scroll-pane
                                             :id "scroll-pane"
                                             :view-order 1
                                             :fit-to-width true
                                             :content {:fx/type :v-box
                                                       :style-class "cljfx-form"
                                                       :children section-views}}}))}}))

(defn- wrap-force-refresh [f view-id]
  (fn [event]
    (f event)
    (g/node-value view-id :form-view)))

(defn- create-renderer [view-id parent workspace project]
  (let [resource-string-converter (make-resource-string-converter workspace)]
    (fx/create-renderer
      :error-handler error-reporting/report-exception!
      :opts {:fx.opt/map-event-handler
             (-> handle-event
                 (fx/wrap-co-effects
                   {:ui-state #(g/node-value view-id :ui-state)
                    :form-data #(g/node-value view-id :form-data)
                    :workspace (constantly workspace)
                    :project (constantly project)
                    :parent (constantly parent)})
                 (fx/wrap-effects
                   {:dispatch fx/dispatch-effect
                    :set (fn [[path value] _]
                           (let [ops (:form-ops (g/node-value view-id :form-data))]
                             (form/set-value! ops path value)))
                    :clear (fn [path _]
                             (let [ops (:form-ops (g/node-value view-id :form-data))]
                               (when (form/can-clear? ops)
                                 (form/clear-value! ops path))))
                    :set-ui-state (fn [ui-state _]
                                    (g/set-property! view-id :ui-state ui-state))
                    :cancel-edit (fn [^Cell cell _]
                                   (fx/run-later (.cancelEdit cell)))
                    :open-resource (fn [[node value] _]
                                     (ui/run-command node :open {:resources [value]}))})
                 (wrap-force-refresh view-id))}

      :middleware (comp
                    fxui/wrap-dedupe-desc
                    (fx/wrap-map-desc
                      (fn [{:keys [form-data ui-state]}]
                        {:fx/type form-view
                         :form-data form-data
                         :ui-state ui-state
                         :parent parent
                         :resource-string-converter resource-string-converter}))))))

(defn- make-form-view [graph parent resource-node opts]
  (let [{:keys [workspace project tab]} opts
        view-id (-> (g/make-nodes graph [view [CljfxFormView]]
                      (g/set-property view :renderer (create-renderer view
                                                                      parent
                                                                      workspace
                                                                      project))
                      (g/connect resource-node :form-data view :form-data))
                    g/transact
                    g/tx-nodes-added
                    first)
        repaint-timer (ui/->timer 30 "refresh-form-view"
                                  (fn [_timer _elapsed]
                                    (g/node-value view-id :form-view)))]
    (ui/timer-start! repaint-timer)
    (ui/timer-stop-on-closed! tab repaint-timer)
    view-id))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :cljfx-form-view
                                :label "Form"
                                :make-view-fn make-form-view))

(handler/defhandler :filter-form :form
  (run [^Node root]
       (when-let [node (.lookup root "#filter-text-field")]
         (.requestFocus node))))

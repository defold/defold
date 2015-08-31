(ns editor.form-view
  (:require [dynamo.graph :as g]
            [editor.form :as form]
            [editor.ui :as ui]
            [editor.dialogs :as dialogs]
            [editor.workspace :as workspace])
  (:import [javafx.animation AnimationTimer]
           [javafx.scene Parent Group]
           [javafx.scene.text Text]
           [javafx.geometry Insets]
           [javafx.scene.layout Pane GridPane HBox VBox]
           [javafx.scene.control ScrollPane TextArea Label TextField CheckBox Button Tooltip]))

(defmulti create-field-control (fn [workspace field-info setter] (:type field-info)))

(defn- tooltip! [ctrl tip]
  (.setTooltip ctrl (Tooltip. tip)))

(defmethod create-field-control :string [_ field-info setter]
  (let [text (TextField.)
        update-ui-fn (fn [value]
                       (ui/text! text (str value)))]
    (ui/on-action! text (fn [_]
                          (setter (ui/text text))))
    (tooltip! text (:help field-info))
    [text update-ui-fn]))

(defn- to-int [str]
  (try
    (Integer/parseInt str)
    (catch Throwable _
      nil)))

(defmethod create-field-control :integer [_ field-info setter]
  (let [text (TextField.)
        update-ui-fn (fn [value]
                       (ui/text! text (str value)))]
    (ui/on-action! text (fn [_]
                          (when-let [v (to-int (ui/text text))]
                            (setter v))))
    (tooltip! text (:help field-info))
    [text update-ui-fn]))

(defn- to-number [str]
  (try
    (Double/parseDouble str)
    (catch Throwable _
      nil)))

(defmethod create-field-control :number [_ field-info setter]
  (let [text (TextField.)
        update-ui-fn (fn [value]
                       (ui/text! text (str value)))]
    (ui/on-action! text (fn [_]
                          (when-let [v (to-number (ui/text text))]
                            (setter v))))
    (tooltip! text (:help field-info))
    [text update-ui-fn]))

(defmethod create-field-control :boolean [_ field-info setter]
  (let [check (CheckBox.)
        update-ui-fn (fn [value]
                       (if (nil? value)
                         (.setIndeterminate check true)
                         (doto check
                           (.setIndeterminate false)
                           (.setSelected value))))]
    (ui/on-action! check (fn [_]
                           (setter (.isSelected check))))
    (tooltip! check (:help field-info))
    [check update-ui-fn]))

(defmethod create-field-control :resource [workspace field-info setter]
  (let [hbox (HBox.)
        button (Button. "...")
        text (TextField.)
        update-ui-fn (fn [value] (ui/text! text value))
        filter (:filter field-info)]
    (ui/on-action! button (fn [_] (when-let [resource (first (dialogs/make-resource-dialog workspace {:ext (when filter [filter])}))]
                                    (let [path (workspace/proj-path resource)
                                          pathc (and path (str path "c"))]
                                      (setter pathc)))))
    (ui/on-action! text (fn [_] (let [path (ui/text text)
                                      resource (or (workspace/find-resource workspace path)
                                                   (workspace/file-resource workspace path))]
                                  (setter (and resource (workspace/proj-path resource))))))
    (ui/children! hbox [text button])
    (tooltip! text (:help field-info))
    [hbox update-ui-fn]))

(defmethod create-field-control :default [_ field-info _]
  (assert false (format "unknown field type %s" field-info)))

(defn- create-field-label [name]
  (let [label (Label. name)]
    label))

(defn- create-field-grid-row [workspace form-ops field-info]
  (let [path (:path field-info)
        label (create-field-label (:label field-info))
        setter (fn [val] (form/set-value! form-ops path val))
        reset-btn (doto (Button. "x")
                    (ui/on-action! (fn [_] (form/clear-value! form-ops path))))
        [control update-ctrl-fn] (create-field-control workspace field-info setter)
        update-ui-fn (fn [annotated-value]
                       (update-ctrl-fn (:value annotated-value))
                       (.setVisible reset-btn (and (not (nil? (:default field-info)))
                                                   (not= (:source annotated-value) :default))))]
    {:ctrls [label control reset-btn] :col-spans [1 1 1] :update-ui-fn [path update-ui-fn]}))

(defn- create-title-label [title]
  (let [label (Label. title)]
    (.setStyle label "-fx-font-size: 16pt; -fx-underline:true;")
    label))

(defn- create-help-text [help]
  (let [label (Label. help)]
    (.setStyle label "-fx-text-fill: #bebebe;")
    label))

(defn- create-section-grid-rows [workspace form-ops section-info]
  (let [{:keys [help title fields]} section-info]
    (when (> (count fields) 0)
      (concat
       (when title
         [{:ctrls [(create-title-label title)] :col-spans [3]}])
       (when help
         [{:ctrls [(create-help-text help)] :col-spans [3]}])
       (map (partial create-field-grid-row workspace form-ops) fields)))))

(defn- update-fields [updaters field-values]
  (doseq [[path val] field-values]
    (let [updater (updaters path)]
      (assert updater (format "missing updater for %s" path))
      (updater val))))

(defn update-form [form form-data]
  (let [updaters (ui/user-data form ::update-ui-fns)]
    (update-fields updaters (:values form-data))
    form))

(defn- add-grid-row [grid row row-data]
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

(defn- create-form [workspace form-data]
  (let [grid-rows (mapcat (partial create-section-grid-rows workspace (:form-ops form-data)) (:sections form-data))
        updaters (into {} (keep :update-ui-fn grid-rows))]
    (let [grid (GridPane.)
          vbox (VBox. (double 10.0))
          scroll-pane (ScrollPane. vbox)]
      (.setHgap grid 4)
      (.setVgap grid 6)
      (add-grid-rows grid grid-rows)
      (ui/children! vbox [grid])
      (.setPadding vbox (Insets. 10 10 10 10))
      (ui/fill-control scroll-pane)
      (ui/user-data! scroll-pane ::update-ui-fns updaters)
      (ui/user-data! scroll-pane ::form-data form-data)
      scroll-pane)))

(defn- same-form-structure [form-data1 form-data2]
  (= (select-keys form-data1 [:form-ops :sections])
     (select-keys form-data2 [:form-ops :sections])))

(g/defnk produce-update-form [parent-view _node-id workspace form-data]
  (let [prev-form (g/node-value _node-id :prev-form)
        prev-form-data (and prev-form (ui/user-data prev-form ::form-data))]
    (if (and prev-form (same-form-structure prev-form-data form-data))
      (update-form prev-form form-data)
      (let [form (create-form workspace form-data)]
        (update-form form form-data)
        (ui/children! parent-view [form])
        (g/set-property! _node-id :prev-form form)
        form))))

(g/defnode FormView
  (property parent-view Parent)
  (property repainter AnimationTimer)
  (property workspace g/Any)
  (property prev-form ScrollPane)
  (input form-data g/Any :substitute {})
  (output form ScrollPane :cached produce-update-form))

(defn- do-make-form-view [graph ^Parent parent resource-node opts]
  (let [workspace (:workspace opts)
        view-id (g/make-node! graph FormView :parent-view parent :workspace workspace)
        repainter (proxy [AnimationTimer] []
                    (handle [now]
                      (g/node-value view-id :form)))]
    (g/transact
     (concat
      (g/set-property view-id :repainter repainter)
      (g/connect resource-node :form-data view-id :form-data)))
    (.start repainter)
    view-id))

(defn- make-form-view [graph ^Parent parent resource-node opts]
  (do-make-form-view graph parent resource-node opts))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :form-view
                                :make-view-fn make-form-view))

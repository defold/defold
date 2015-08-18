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

(defmethod create-field-control String [_ field-info setter]
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

(defmethod create-field-control g/Int [_ field-info setter]
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

(defmethod create-field-control g/Num [_ field-info setter]
  (let [text (TextField.)
        update-ui-fn (fn [value]
                       (ui/text! text (str value)))]
    (ui/on-action! text (fn [_]
                          (when-let [v (to-number (ui/text text))]
                            (setter v))))
    (tooltip! text (:help field-info))
    [text update-ui-fn]))

(defmethod create-field-control g/Bool [_ field-info setter]
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

(defn- create-field-grid-row [workspace ctxt section field-info]
  (let [field (:field field-info)
        label (create-field-label field)
        setter (fn [val] (form/set-value! ctxt section field val))
        reset-btn (doto (Button. "x")
                    (ui/on-action! (fn [_] (form/clear-value! ctxt section field))))
        [control update-ctrl-fn] (create-field-control workspace field-info setter)
        update-ui-fn (fn [annotated-value]
                       (update-ctrl-fn (:value annotated-value))
                       (.setVisible reset-btn (and (not (nil? (:default field-info)))
                                                   (not= (:source annotated-value) :default))))]
    {:ctrls [label control reset-btn] :col-spans [1 1 1] :update-ui-fn {[section field] update-ui-fn}}))

(defn- create-title-label [title]
  (let [label (Label. title)]
    label))

(defn- create-help-text [help]
  (let [text (Text. help)]
    text))

(defn- create-section-grid-rows [workspace ctxt section-info]
  (let [{:keys [section help title fields]} section-info]
    (when (> (count fields) 0)
      (concat
       (when title
         [{:ctrls [(create-title-label title)] :col-spans [3]}])
       (when help
         [{:ctrls [(create-help-text help)] :col-spans [3]}])
       (map (partial create-field-grid-row workspace ctxt section) fields)))))

(defn- update-fields [updaters field-values]
  (doseq [[section field-vals] field-values]
    (doseq [[field val] field-vals]
      (let [updater (updaters [section field])]
        (assert updater (format "missing updater for %s" [section field]))
        (updater val)))))

(defn update-form [form form-data]
  (let [updaters (ui/user-data form ::update-ui-fns)]
    (update-fields updaters (form/field-values-and-defaults form-data))))

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
  (let [grid-rows (mapcat (partial create-section-grid-rows workspace (:ctxt form-data)) (:sections form-data))
        updaters (remove nil? (map :update-ui-fn grid-rows))
        merged-updaters (reduce merge {} updaters)]
    (let [grid (GridPane.)
          vbox (VBox. (double 10.0))
          scroll-pane (ScrollPane. vbox)]
      (.setHgap grid 4)
      (.setVgap grid 6)
      (add-grid-rows grid grid-rows)
      (ui/children! vbox [grid])
      (.setPadding vbox (Insets. 10 10 10 10))
      (ui/fill-control scroll-pane)
      (ui/user-data! scroll-pane ::update-ui-fns merged-updaters)
      (ui/user-data! scroll-pane ::form-data form-data)
      scroll-pane)))

(g/defnk produce-update-form [parent-view _node-id workspace form-data]
  (let [prev-form (g/node-value _node-id :prev-form)
        prev-form-data (and prev-form (ui/user-data prev-form ::form-data))]
    (if (and prev-form
             (= (select-keys form-data [:ctxt :sections])
                (select-keys prev-form-data [:ctxt :sections])))
      (do
        (update-form prev-form form-data)
        prev-form)
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
  (input form-data g/Any)
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

(ns internal.ui.property
  (:require [camel-snake-kebab :refer :all]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [dynamo.messages :as msg]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.property :as dp]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.ui.widgets :as widgets]
            [dynamo.util :refer :all]
            [internal.node :as in]
            [internal.system :as is]
            [plumbing.core :refer [defnk]]
            [schema.core :as s]
            [service.log :as log]))

(defrecord ValidationPresenter []
  dp/Presenter
  (control-for-property [this]
    {:type :status-label :style :border #_:status #_Status/OK_STATUS :layout-data {:min-width 50 :exclude true}})
  (settings-for-control [_ value]
    (if (empty? value)
      {#_:status #_Status/OK_STATUS :layout-data {:exclude true} :visible false}
      {#_:status #_(marker/error-status (str/join "\n" value)) :layout-data {:min-width 50 :exclude false} :visible true})))

(def validation-presenter (->ValidationPresenter))

(defrecord FillerPresenter []
  dp/Presenter
  (control-for-property [this]
    {:type :label :layout-data {:exclude true}})
  (settings-for-control [_ value]
    (if (empty? value)
      {:layout-data {:exclude true}  :visible false}
      {:layout-data {:exclude false} :visible true})))

(def filler-presenter (->FillerPresenter))

(defn- prop-name-modifier [suffix prop-name]
  (keyword (str (name prop-name) suffix)))
(def ^:private prop-filler-label (partial prop-name-modifier "-filler"))
(def ^:private prop-status-label (partial prop-name-modifier "-status"))

(defnk aggregate-properties
  [properties]
  (into {}
    (for [node-prop-map        properties
          [prop-name prop]     node-prop-map
          :when (some-> prop :type t/property-visible)]
      [prop-name prop])))

(defnk passthrough-presenter-registry
  [presenter-registry]
  presenter-registry)

(defn presenter-for-property [node property]
  (dp/lookup-presenter (g/node-value node :presenter-registry) (:type property)))

(defn- attach-user-data
  [spec node prop-name presenter path]
  (assoc spec
    :user-data {:property-view-node node
                :project-node (p/project-root-node node)
                :presenter presenter
                :prop-name prop-name
                :path path}
    :children  (mapv (fn [[child-name child-spec]] [child-name (attach-user-data child-spec node prop-name presenter (conj path child-name))]) (:children spec))))

(defn- attach-listeners
  [spec ui-event-listener]
  (assoc spec
    :listen   (zipmap (:listen spec) (repeat ui-event-listener))
    :children (mapv (fn [[child-name child-spec]] [child-name (attach-listeners child-spec ui-event-listener)]) (:children spec))))

(defn- control-spec
  [node prop-name presenter]
  (-> (merge (dp/control-for-property presenter))
      (attach-user-data node prop-name presenter [])
      (attach-listeners (:ui-event-listener node))))

(defn- prop-label
  [prop-name label-text]
  {:type :composite
   :layout {:type :stack}
   :children [[:label      {:type :label :text label-text}]
              [:label-link {:type :hyperlink :text label-text :underlined true :on-click (fn [_] (prn "RESET " prop-name)) :foreground [0 0 255] :tooltip-text msg/FormPropertySheetViewer_RESET_VALUE}]]})

(defn- property-control-strip
  [node [prop-name {:keys [presenter]}]]
  (let [label-text (keyword->label prop-name)]
    [[:label-stack {:type :stack
                    :children [[:label      {:type :label :text label-text}]
                               [:label-link {:type :hyperlink :text label-text :underlined true :on-click (fn [_] (prn "RESET " prop-name)) :foreground [0 0 255] :tooltip-text msg/FormPropertySheetViewer_RESET_VALUE}]]}]
     [prop-name (control-spec node prop-name presenter)]
     [:dummy           {:type :label :layout-data {:exclude true}}]
;;     [:status-label    {:type :status-label :style :border :status Status/OK_STATUS :layout-data {:min-width 50 :exclude true}}]
     ]))

(defn- property-page
  [control-strips]
  [:page-content
   {:type :grid
    ;; :layout {:type :grid :margin-width 0 :columns [{:horizontal-alignment :left} {:horizontal-alignment :fill}]}
    :children control-strips}])

(defn- make-property-page
  [node properties-form properties]
  (widgets/make-control (widgets/widget properties-form [:form :composite])
    (property-page (mapcat #(property-control-strip node %) properties))))

(def empty-property-page
  [:page-content
   {:type :grid
    :layout {:type :grid :columns [{:horizontal-alignment :left}]}
    :children [[:no-selection-label {:type :label :text msg/FormPropertySheetViewer_NO_PROPERTIES}]]}])

(defn- make-empty-property-page
  [properties-form]
  (widgets/make-control (widgets/widget properties-form [:form :composite]) empty-property-page))

(defn- settings-for-page
  [properties]
  {:children
   (concat
     (for [[prop-name {:keys [presenter value]}] properties]
       [prop-name (dp/settings-for-control presenter value)])
     (for [[prop-name {:keys [validation-problems]}] properties]
       [(prop-filler-label prop-name) (dp/settings-for-control filler-presenter validation-problems)])
     (for [[prop-name {:keys [validation-problems]}] properties]
       [(prop-status-label prop-name) (dp/settings-for-control validation-presenter validation-problems)]))})

(defn- cache-key
  [properties]
  (map-vals :type properties))

(defn- lookup-or-create [cache key f & args]
  (-> cache
      (swap! (fn [cache]
                (if (contains? cache key)
                  cache
                  (assoc cache key (apply f args)))))
      (get key)))

(defn- attach-presenters
  [node content]
  (map-vals #(assoc % :presenter (presenter-for-property node %)) content))

(defn- refresh-property-page
  [{:keys [sheet-cache properties-form] :as node}]
  (let [content (attach-presenters node (g/node-value node :content))
        key     (cache-key content)
        page    (lookup-or-create sheet-cache key make-property-page node properties-form content)]
    (widgets/update-ui!      (get-in page [:page-content]) (settings-for-page content))
    (widgets/bring-to-front! (widgets/widget page [:page-content]))
    (widgets/scroll-to-top!  (widgets/widget properties-form [:form]))))

(def gui
  [:form {:type   :form
          :text   "Properties"
          :layout {:type :grid :columns [{:horizontal-alignment :fill :vertical-alignment :fill}]}
          :children [[:composite
                      {:type :composite
                       :layout {:type :stack}}]]}])

(n/defnode PropertyView
  (input  properties [t/Properties])
  (output content s/Any aggregate-properties)

  (input  presenter-registry t/Registry :inject)
  (output presenter-registry t/Registry passthrough-presenter-registry)

  (on :create
    (let [properties-form   (widgets/make-control (:parent event) gui)
          sheet-cache       (atom {})
          ;;ui-event-listener (ui/make-listener #(n/dispatch-message self :ui-event :ui-event %) [])
          ]
      (lookup-or-create sheet-cache (cache-key {}) make-empty-property-page properties-form)
      (ds/set-property self
        :sheet-cache       sheet-cache
        :properties-form   properties-form)))

  #_(on :ui-event
    (let [ui-event (:ui-event event)
          {:keys [presenter prop-name path]} (widgets/get-user-data (:widget ui-event))
          content (g/node-value self :content)
          page (get @(:sheet-cache self) (cache-key content))
          widget-subtree (get-in page [:page-content prop-name])]
      (if (identical? (:widget ui-event) (widgets/widget widget-subtree path))
        (let [prop (get content prop-name)
              presenter-event (dp/presenter-event-map ui-event)
              old-value (:value prop)
              result (dp/on-event presenter widget-subtree path presenter-event old-value)]
          (when-let [new-value (:value result)]
            (when (not= new-value old-value)
              (ds/tx-label (str "Set " (keyword->label prop-name)))
              (ds/set-property {:_id (:node-id prop)} prop-name new-value))))
        (log/warn :message "Expected event from widget on active property page"))))

;;  ISelectionListener
  #_(selectionChanged [this part selection]
    (let [current-inputs (ds/sources-of this :properties)]
      (when (not= @selection (map (comp :_id first) current-inputs))
        (ds/transactional
          (doseq [[source-node source-label] current-inputs]
            (ds/disconnect source-node source-label this :properties))
          (doseq [n @selection]
            (ds/connect {:_id n} :properties this :properties)))))))

#_(defn implementation-for
  [scope]
  (ds/transactional
    (ds/in scope
      (ds/add (n/construct PropertyView)))))

#_(defn get-control
  "This is called by the Java shim GenericPropertySheetPage. Not for other use."
  [property-view-node]
  (-> property-view-node
      ds/refresh
      :properties-form
      (widgets/widget [:form])))

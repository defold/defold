(ns editor.graph-view
  (:require [cljfx.api :as fx]
            [cljfx.fx.circle :as fx.circle]
            [cljfx.fx.group :as fx.group]
            [cljfx.fx.rectangle :as fx.rectangle]
            [cljfx.fx.scene :as fx.scene]
            [cljfx.fx.scroll-pane :as fx.scroll-pane]
            [cljfx.fx.text :as fx.text]
            [cljfx.fx.v-box :as fx.v-box]
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.ui :as ui]
            [internal.node :as in])
  (:import [java.util Base64]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; TODO: Move to styling/stylesheets/graph-view.sass.
(def ^:private ^String inline-stylesheet "
.graph-view-toolbar {
  -fx-background-color: -df-component
}

.graph-view-scroll-view {
  -fx-background-color: -df-background;
}

.graph-view-node {
  -fx-fill: -df-component;
}

.graph-view-node-title Rectangle {
  -fx-fill: -df-defold-green;
}

.graph-view-node-title Text {
  -fx-fill: white;
}

.graph-view-node-property Text {
  -fx-fill: -df-text;
}

.graph-view-node-property Circle {
  -fx-fill: -df-background;
}

.graph-view-node-input Text {
  -fx-fill: -df-defold-yellow;
}

.graph-view-node-input Circle {
  -fx-fill: -df-background;
}

.graph-view-node-output Text {
  -fx-fill: -df-defold-blue;
}

.graph-view-node-output Circle {
  -fx-fill: -df-background;
}
")

(def ^:private inline-stylesheet-data-url
  (str "data:text/css;base64,"
       (.encodeToString (Base64/getUrlEncoder)
                        (.getBytes inline-stylesheet))))

(def ^:private ^{:tag 'long} graph-view-node-width 200)
(def ^:private ^{:tag 'long} graph-view-node-label-height 20)

(def ^:private graph-view-node-label-clip
  {:fx/type fx.rectangle/lifecycle
   :width graph-view-node-width
   :height graph-view-node-label-height})

(defn- graph-view-node-title [node-type]
  {:fx/type fx.group/lifecycle
   :fx/key :node/title
   :style-class "graph-view-node-title"
   :layout-y (- graph-view-node-label-height)
   :children [{:fx/type fx.rectangle/lifecycle
               :width graph-view-node-width
               :height graph-view-node-label-height}
              {:fx/type fx.text/lifecycle
               :x 20
               :y 14
               :text (-> node-type :k name)
               :clip graph-view-node-label-clip}]})

(defn- graph-view-node-property [^long label-index property-label]
  {:fx/type fx.group/lifecycle
   :fx/key (keyword "property" (name property-label))
   :style-class "graph-view-node-property"
   :layout-y (* label-index graph-view-node-label-height)
   :children [{:fx/type fx.text/lifecycle
               :y 14
               :wrapping-width (- graph-view-node-width 20)
               :text-alignment :right
               :text (name property-label)
               :clip graph-view-node-label-clip}
              {:fx/type fx.circle/lifecycle
               :center-x (- graph-view-node-width 10)
               :center-y (/ graph-view-node-label-height 2)
               :radius 5}]})

(defn- graph-view-node-input [^long label-index [input-label input-info]]
  {:fx/type fx.group/lifecycle
   :fx/key (keyword "input" (name input-label))
   :style-class "graph-view-node-input"
   :layout-y (* label-index graph-view-node-label-height)
   :children [{:fx/type fx.text/lifecycle
               :x 20
               :y 14
               :text (name input-label)
               :clip graph-view-node-label-clip}
              {:fx/type fx.circle/lifecycle
               :center-x 10
               :center-y (/ graph-view-node-label-height 2)
               :radius 5}]})

(defn- graph-view-node-output [^long label-index [output-label output-info]]
  {:fx/type fx.group/lifecycle
   :fx/key (keyword "output" (name output-label))
   :style-class "graph-view-node-output"
   :layout-y (* label-index graph-view-node-label-height)
   :children [{:fx/type fx.text/lifecycle
               :y 14
               :wrapping-width (- graph-view-node-width 20)
               :text-alignment :right
               :text (name output-label)
               :clip graph-view-node-label-clip}
              {:fx/type fx.circle/lifecycle
               :center-x (- graph-view-node-width 10)
               :center-y (/ graph-view-node-label-height 2)
               :radius 5}]})

(defn- graph-view-node [{:keys [node-type position] :as props}]
  (let [declared-inputs (in/declared-inputs node-type)
        declared-outputs (in/declared-outputs node-type)
        declared-property-labels (in/declared-property-labels node-type)
        fake-output? (comp (conj declared-property-labels :_output-jammers) key)
        real-outputs (remove fake-output? declared-outputs)
        inputs (sort-by key declared-inputs)
        outputs (sort-by key real-outputs)
        property-labels (sort declared-property-labels)
        property-count (count property-labels)
        input-count (count inputs)
        output-count (count outputs)
        label-count (+ input-count output-count property-count)
        height (* label-count graph-view-node-label-height)]
    (-> props
        (dissoc :node-type :position)
        (assoc :fx/type fx.group/lifecycle
               :layout-x (position 0)
               :layout-y (position 1)
               :children (into [{:fx/type fx.rectangle/lifecycle
                                 :fx/key :node/background
                                 :style-class "graph-view-node"
                                 :width graph-view-node-width
                                 :height height}
                                (graph-view-node-title node-type)]
                               (concat
                                 (map graph-view-node-input
                                      (range)
                                      inputs)
                                 (map graph-view-node-output
                                      (range input-count Long/MAX_VALUE)
                                      outputs)
                                 (map graph-view-node-property
                                      (range (+ input-count output-count) Long/MAX_VALUE)
                                      property-labels)))))))

(defn- graph-view-map-desc [graph-view-data]
  {:fx/type fxui/stage
   :title "Graph View"
   :modality :none
   :showing true
   :scene {:fx/type fx.scene/lifecycle
           :stylesheets ["dialogs.css" inline-stylesheet-data-url]
           :root {:fx/type fx.v-box/lifecycle
                  :children [{:fx/type fx.v-box/lifecycle
                              :style-class "graph-view-toolbar"
                              :children [{:fx/type fxui/button
                                          :text "Close"
                                          :cancel-button true
                                          :on-action {:event-type :close-window}}]}
                             {:fx/type fx.scroll-pane/lifecycle
                              :style-class "graph-view-scroll-view"
                              :v-box/vgrow :always
                              :pref-width 800
                              :pref-height 600
                              :pannable true
                              :content {:fx/type fx.group/lifecycle
                                        :children (into []
                                                        (map (fn [[node-id graph-view-node-props]]
                                                               (assoc graph-view-node-props
                                                                 :fx/type graph-view-node
                                                                 :fx/key node-id)))
                                                        graph-view-data)}}]}}})

(defn- mount-graph-view-window! [graph-view-data-atom]
  (let [renderer-ref (volatile! nil)
        renderer (fx/create-renderer
                   :error-handler error-reporting/report-exception!
                   :middleware (fx/wrap-map-desc graph-view-map-desc)
                   :opts {:fx.opt/map-event-handler
                          (fn [event]
                            (case (:event-type event)
                              :close-window (fx/unmount-renderer graph-view-data-atom @renderer-ref)))})]
    (vreset! renderer-ref renderer)
    (fx/mount-renderer graph-view-data-atom renderer)))

(defn- graph-view-data-for-node [evaluation-context node-id]
  (let [basis (:basis evaluation-context)
        node-type (g/node-type* basis node-id)]
    {:node-type node-type}))

(defn- initial-position-for-node [graph-view-data node-data]
  ;; TODO: Choose a suitable location based on topology and placement of existing nodes.
  (let [column-width (+ graph-view-node-width 100)
        x (* column-width (count graph-view-data))
        y ^long (or (some-> (first graph-view-data) val :position second) 0)]
    [x y]))

(defn- refresh-graph-view-data [graph-view-data evaluation-context]
  ;; We don't want to use a transient here, since we want to return the
  ;; identical graph-view-data if there are no changes.
  (reduce (fn [graph-view-data [node-id node-info]]
            (assoc graph-view-data node-id (merge node-info (graph-view-data-for-node evaluation-context node-id))))
          graph-view-data
          graph-view-data))

(defn- add-node [evaluation-context graph-view-data node-id]
  (assert (g/node-id? node-id))
  (let [graph-view-data (refresh-graph-view-data graph-view-data evaluation-context)]
    (if (contains? graph-view-data node-id)
      graph-view-data
      (let [node-data (graph-view-data-for-node evaluation-context node-id)
            node-position (initial-position-for-node graph-view-data node-data)
            node-data-with-position (assoc node-data :position node-position)]
        (assoc graph-view-data node-id node-data-with-position)))))

(defonce graph-view-data-atom (atom (sorted-map)))

(defn show-graph-view-window! []
  (ui/run-later
    (mount-graph-view-window! graph-view-data-atom)))

(defn add-node! [node-id]
  (ui/run-now
    (let [old-graph-view-data @graph-view-data-atom
          new-graph-view-data (g/with-auto-evaluation-context evaluation-context
                                (add-node evaluation-context old-graph-view-data node-id))]
      (when-not (identical? old-graph-view-data new-graph-view-data)
        (reset! graph-view-data-atom new-graph-view-data)))))

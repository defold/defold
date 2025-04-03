(ns editor.prefs-dialog
  (:require [cljfx.api :as fx]
            [cljfx.fx.column-constraints :as fx.column-constraints]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.scene :as fx.scene]
            [cljfx.fx.tab :as fx.tab]
            [cljfx.fx.tab-pane :as fx.tab-pane]
            [clojure.java.io :as io]
            [editor.fxui :as fxui]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [javafx.scene Scene]
           [javafx.scene.input KeyCode KeyEvent]))

(def pages
  (delay
    (cond->
      [{:name "General"
        :paths [[:workflow :load-external-changes-on-app-focus]
                [:bundle :open-output-directory]
                [:build :open-html5-build]
                [:build :texture-compression]
                [:run :quit-on-escape]
                [:asset-browser :track-active-tab]
                [:build :lint-code]
                [:input :keymap-path]
                [:run :engine-arguments]]}
       {:name "Code"
        :paths [[:code :custom-editor]
                [:code :open-file]
                [:code :open-file-at-line]
                [:code :font :name]
                [:code :zoom-on-scroll]]}
       {:name "Extensions"
        :paths [[:extensions :build-server]
                [:extensions :build-server-username]
                [:extensions :build-server-password]
                [:extensions :build-server-headers]]}
       {:name "Tools"
        :paths [[:tools :adb-path]
                [:tools :ios-deploy-path]]}]

      (system/defold-dev?)
      (conj {:name "Dev"
             :paths [[:dev :custom-engine]]}))))

(defmulti form-input (fn [schema _value _on-value-changed]
                       (or (:type (:ui schema))
                           (:type schema))))

(defmethod form-input :default [_ value _]
  {:fx/type fxui/label
   :text (str value)})

(defmethod form-input :boolean [_ value on-value-changed]
  {:fx/type fx.h-box/lifecycle
   :children [{:fx/type fxui/check-box
               :selected value
               :on-selected-changed on-value-changed}]})

(defmethod form-input :string [schema value on-value-changed]
  (let [{:keys [prompt multiline]} (:ui schema)]
    (cond-> {:fx/type (if multiline fxui/value-area fxui/value-field)
             :value value
             :on-value-changed on-value-changed}
            prompt (assoc :prompt-text prompt))))

(defmethod form-input :password [schema value on-value-changed]
  (let [prompt (-> schema :ui :prompt)]
    (cond-> {:fx/type fxui/password-value-field
             :value value
             :on-value-changed on-value-changed}
            prompt (assoc :prompt-text prompt))))

(defn- prefs-tab-pane [{:keys [prefs] prefs-state :value}]
  {:fx/type fx.tab-pane/lifecycle
   :tab-closing-policy :unavailable
   :tabs (mapv
           (fn [{:keys [name paths]}]
             {:fx/type fx.tab/lifecycle
              :text name
              :content {:fx/type fxui/grid
                        :padding :medium
                        :spacing :medium
                        :column-constraints [{:fx/type fx.column-constraints/lifecycle}
                                             {:fx/type fx.column-constraints/lifecycle
                                              :hgrow :always}]
                        :children (->> paths
                                       (coll/mapcat-indexed
                                         (fn [row path]
                                           (let [schema (prefs/schema prefs-state prefs path)
                                                 tooltip (:description (:ui schema))]
                                             [(cond-> {:fx/type fxui/label
                                                       :grid-pane/row row
                                                       :grid-pane/column 0
                                                       :grid-pane/fill-width false
                                                       :grid-pane/fill-height false
                                                       :grid-pane/valignment :top
                                                       :text (prefs/label prefs-state prefs path)}
                                                      tooltip
                                                      (assoc :tooltip tooltip))
                                              (assoc
                                                (form-input schema
                                                            (prefs/get prefs-state prefs path)
                                                            (fn/partial prefs/set! prefs path))
                                                :grid-pane/row row
                                                :grid-pane/column 1)])))
                                       vec)}})
           @pages)})

(defn handle-scene-key-pressed [^KeyEvent e]
  (when (= KeyCode/ESCAPE (.getCode e))
    (.consume e)
    (.hide (.getWindow ^Scene (.getSource e)))))

(defn open!
  "Show the prefs dialog and block the thread until the dialog is closed"
  [prefs]
  (fxui/show-stateless-dialog-and-await-result!
    (fn [result-fn]
      {:fx/type fxui/dialog-stage
       :showing true
       :on-hidden result-fn
       :title "Preferences"
       :resizable true
       :min-width 650
       :min-height 500
       :scene {:fx/type fx.scene/lifecycle
               :stylesheets [(str (io/resource "dialogs.css"))]
               :on-key-pressed handle-scene-key-pressed
               :root {:fx/type fx/ext-watcher
                      :ref prefs/global-state
                      :desc {:fx/type prefs-tab-pane
                             :prefs prefs}}}}))
  nil)

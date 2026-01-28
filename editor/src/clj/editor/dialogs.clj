;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.dialogs
  (:require [cljfx.api :as fx]
            [cljfx.ext.list-view :as fx.ext.list-view]
            [cljfx.composite :as fx.composite]
            [cljfx.fx.group :as fx.group]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.hyperlink :as fx.hyperlink]
            [cljfx.fx.image-view :as fx.image-view]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.progress-bar :as fx.progress-bar]
            [cljfx.fx.progress-indicator :as fx.progress-indicator]
            [cljfx.fx.region :as fx.region]
            [cljfx.fx.scene :as fx.scene]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.v-box :as fx.v-box]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.prop :as fx.prop]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.error-reporting :as error-reporting]
            [editor.field-expression :as field-expression]
            [editor.fxui :as fxui]
            [editor.github :as github]
            [editor.localization :as localization]
            [editor.os :as os]
            [editor.progress :as progress]
            [editor.ui :as ui]
            [editor.util :as util]
            [service.log :as log]
            [util.coll :as coll]
            [util.path :as path]
            [util.thread-util :as thread-util])
  (:import [clojure.lang Named]
           [com.defold.control ClippingContainer]
           [java.io File]
           [java.nio.file Path Paths]
           [java.util Collection List]
           [javafx.application Platform]
           [javafx.collections ListChangeListener]
           [javafx.event Event]
           [javafx.scene.control ListView TextField]
           [javafx.scene.input KeyCode KeyEvent MouseButton MouseEvent]
           [javafx.stage DirectoryChooser FileChooser FileChooser$ExtensionFilter Screen Stage Window]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def clipping-container
  (fx.composite/describe ClippingContainer :ctor [] :props fx.stack-pane/props))

(def ^:private dialogs-css-delay
  (delay (str (io/resource "dialogs.css"))))

(defn max-dialog-stage-height []
  (let [screens (Screen/getScreens)
        n (.size screens)]
    (loop [i 0
           max-height 0.0]
      (if (= i n)
        max-height
        (recur (unchecked-inc i) (max max-height (.getHeight (.getVisualBounds ^Screen (.get screens i)))))))))

(defn dialog-stage
  "Dialog `:stage` that manages scene graph itself and provides layout common
  for many dialogs.

  Required keys:

    :header    cljfx description, header of a dialog, already padded
    :footer    cljfx description, footer of a dialog, already padded

  Optional keys:

    :size          dialog width, either :small, :default or :large
    :content       a content of a dialog, not padded; you can use
                   \"dialog-content-padding\" style class to set desired padding
                   (or \"text-area-with-dialog-content-padding\" for text areas)
    :root-props    extra props to scene root's v-box lifecycle (sans :children)"
  [{:keys [size header content footer root-props]
    :or {size :default root-props {}}
    :as props}]
  (-> props
      (dissoc :size :header :content :footer :root-props)
      (assoc :fx/type fxui/dialog-stage
             :max-height (max-dialog-stage-height)
             :scene {:fx/type fx.scene/lifecycle
                     :stylesheets [@dialogs-css-delay]
                     :root (-> root-props
                               (fxui/add-style-classes
                                 "dialog-body"
                                 (case size
                                   :small "dialog-body-small"
                                   :default "dialog-body-default"
                                   :large "dialog-body-large"))
                               (assoc :fx/type fx.v-box/lifecycle
                                      :children (if (some? content)
                                                  [{:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-with-content-header"
                                                    :children [header]}
                                                   {:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-content"
                                                    :children [{:fx/type clipping-container
                                                                :max-height (case size
                                                                              :small 480.0
                                                                              :default 600.0
                                                                              :large 720.0)
                                                                :children [content]}]}
                                                   {:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-with-content-footer"
                                                    :children [footer]}]
                                                  [{:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-without-content-header"
                                                    :children [header]}
                                                   {:fx/type fx.region/lifecycle :style-class "dialog-no-content"}
                                                   {:fx/type fx.v-box/lifecycle
                                                    :style-class "dialog-without-content-footer"
                                                    :children [footer]}])))})))

(defn- confirmation-dialog-header->fx-desc [localization header]
  {:pre [header]}
  (if (or (string? header) (localization/message-pattern? header))
    {:fx/type fxui/legacy-label
     :variant :header
     :text (localization header)}
    header))

(defn dialog-buttons [props]
  (-> props
      (assoc :fx/type fx.h-box/lifecycle)
      (util/provide-defaults :alignment :center-right)
      (fxui/add-style-classes "spacing-smaller")))

(defn- confirmation-dialog [{:keys [buttons icon localization]
                             :or {icon ::no-icon}
                             :as props}]
  (let [button-descs (mapv (fn [button-props]
                             (let [button-desc (-> button-props
                                                   (assoc :fx/type fxui/legacy-button
                                                          :on-action {:result (:result button-props)})
                                                   (update :text localization)
                                                   (dissoc :result))]
                               (if (:default-button button-props)
                                 {:fx/type fxui/ext-focused-by-default
                                  :desc (util/provide-defaults button-desc :variant :primary)}
                                 button-desc)))
                           buttons)]
    (-> props
        (assoc :fx/type dialog-stage
               :showing (fxui/dialog-showing? props)
               :footer {:fx/type dialog-buttons
                        :children button-descs}
               :on-close-request {:result (:result (some #(when (:cancel-button %) %) buttons))})
        (dissoc :buttons :icon ::fxui/result :localization)
        (update :title localization)
        (update :header (fn [header]
                          (let [header-desc (confirmation-dialog-header->fx-desc localization header)]
                            (if (= icon ::no-icon)
                              header-desc
                              {:fx/type fx.h-box/lifecycle
                               :style-class "spacing-smaller"
                               :alignment :center-left
                               :children [(if (keyword? icon)
                                            {:fx/type fxui/icon :type icon}
                                            icon)
                                          header-desc]})))))))

(defn content-text-area [props]
  (-> props
      (assoc :fx/type fxui/legacy-text-area)
      (fxui/add-style-classes "text-area-with-dialog-content-padding")
      (util/provide-defaults
        :pref-row-count (max 3 (count (string/split (:text props "") #"\n" 10)))
        :variant :borderless
        :editable false)))

(defn- coerce-dialog-content [content localization]
  (cond
    (:fx/type content)
    content

    (or (string? content) (localization/message-pattern? content))
    {:fx/type content-text-area
     :text (localization content)}

    (map? content)
    (-> content
        (assoc :fx/type content-text-area)
        (update :text localization))))

(defn make-confirmation-dialog
  "Shows a dialog and blocks current thread until users selects one option.

  props is a prop map to configure the dialog, supports all options from
  [[dialog-stage]] with these changes:
    :title      can be a MessagePattern in addition to string
    :header     can be a string or MessagePattern in addition to cljfx desc
    :footer     not supported; use :buttons instead
    :content    can be:
                  * cljfx description (used as is)
                  * prop map (map without :fx/type key) for
                    [[editor.fxui/text-area]] - readonly by default to allow
                    user select and copy text; :text prop is required, can be a
                    MessagePattern in addition to string
                  * string - text or MessagePattern for readonly text area
    :buttons    use instead of :footer; a coll of button descriptions, where
                a button description is a prop map for [[editor.fxui/button]]
                with a few caveats:
                  * you don't have to specify :fx/type
                  * it should have :result key; its value will be returned from
                    this function (default nil)
                  * if you specify :default-button, it will be styled as primary
                    and receive focus by default
                  * if you specify :cancel-button, dismissing the window will
                    return :result from that button (and nil otherwise)
                  * :text prop will be localized (can be a MessagePattern in
                    addition to string)"
  [localization props]
  (fxui/show-dialog-and-await-result!
    :event-handler (fn [state event]
                     (assoc state ::fxui/result (:result event)))
    :description (-> props
                     (assoc :fx/type confirmation-dialog
                            :localization localization)
                     (update :content coerce-dialog-content localization))))

(def ^String indented-bullet
  ;; "  * " (NO-BREAK SPACE, NO-BREAK SPACE, BULLET, NO-BREAK SPACE)
  "\u00A0\u00A0\u2022\u00A0")

(def indent-with-bullet (partial str indented-bullet))

(defn make-info-dialog
  "Shows a dialog with selectable text content and blocks current thread until
  user closes it.

  props is a map to configure the dialog, supports all options from
  [[make-confirmation-dialog]] with these changes:
    * :buttons include a close button by default"
  [localization props]
  (make-confirmation-dialog
    localization
    (-> props
        (util/provide-defaults :buttons [{:text (localization/message "dialog.button.close")
                                          :cancel-button true
                                          :default-button true}]))))

(defn- digit-string? [^String x]
  (and (string? x)
       (pos? (.length x))
       (every? #(Character/isDigit ^char %) x)))

(defn resolution-dialog [{:keys [width-text height-text localization] :as props}]
  (let [width-valid (digit-string? width-text)
        height-valid (digit-string? height-text)]
    {:fx/type dialog-stage
     :showing (fxui/dialog-showing? props)
     :on-close-request {:event-type :cancel}
     :title (localization (localization/message "dialog.custom-resolution.title"))
     :size :small
     :header {:fx/type fx.v-box/lifecycle
              :children [{:fx/type fxui/legacy-label
                          :variant :header
                          :text (localization (localization/message "dialog.custom-resolution.header"))}
                         {:fx/type fxui/legacy-label
                          :text (localization (localization/message "dialog.custom-resolution.detail"))}]}
     :content {:fx/type fxui/two-col-input-grid-pane
               :style-class "dialog-content-padding"
               :children [{:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.custom-resolution.label.width"))}
                          {:fx/type fxui/legacy-text-field
                           :variant (if width-valid :default :error)
                           :text width-text
                           :on-text-changed {:event-type :set-width}}
                          {:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.custom-resolution.label.height"))}
                          {:fx/type fxui/legacy-text-field
                           :variant (if height-valid :default :error)
                           :text height-text
                           :on-text-changed {:event-type :set-height}}]}
     :footer {:fx/type dialog-buttons
              :children [{:fx/type fxui/legacy-button
                          :cancel-button true
                          :on-action {:event-type :cancel}
                          :text (localization (localization/message "dialog.button.cancel"))}
                         {:fx/type fxui/legacy-button
                          :variant :primary
                          :disable (or (not width-valid) (not height-valid))
                          :default-button true
                          :text (localization (localization/message "dialog.custom-resolution.button.set-resolution"))
                          :on-action {:event-type :confirm}}]}}))

(defn make-resolution-dialog [data localization]
  (fxui/show-dialog-and-await-result!
    :initial-state {:width-text (str (or (:width data) "320"))
                    :height-text (str (or (:height data) "420"))}
    :event-handler (fn [state {:keys [fx/event event-type]}]
                     (case event-type
                       :set-width (assoc state :width-text event)
                       :set-height (assoc state :height-text event)
                       :cancel (assoc state ::fxui/result nil)
                       :confirm (assoc state ::fxui/result {:width (field-expression/to-int (:width-text state))
                                                            :height (field-expression/to-int (:height-text state))})))
    :description {:fx/type resolution-dialog
                  :localization localization}))

(defn make-update-failed-dialog [^Stage owner localization]
  (let [result (make-confirmation-dialog
                 localization
                 {:title (localization/message "updater.update-failed.title")
                  :owner owner
                  :icon :icon/triangle-error
                  :header {:fx/type fx.v-box/lifecycle
                           :children [{:fx/type fxui/legacy-label
                                       :variant :header
                                       :text (localization (localization/message "updater.update-failed.header"))}
                                      {:fx/type fxui/legacy-label
                                       :text (localization (localization/message "updater.update-failed.detail"))}]}
                  :buttons [{:text (localization/message "dialog.button.quit")
                             :cancel-button true
                             :result false}
                            {:text (localization/message "dialog.button.open-defold-website")
                             :default-button true
                             :result true}]})]
    (when result
      (ui/open-url "https://www.defold.com/"))))

(defn make-download-update-or-restart-dialog [^Stage owner localization]
  (make-confirmation-dialog
    localization
    {:title (localization/message "updater.download-or-restart-dialog.title")
     :icon :icon/circle-info
     :size :large
     :owner owner
     :header {:fx/type fx.v-box/lifecycle
              :children [{:fx/type fxui/legacy-label
                          :variant :header
                          :text (localization (localization/message "updater.download-or-restart-dialog.header"))}
                         {:fx/type fxui/legacy-label
                          :text (localization (localization/message "updater.download-or-restart-dialog.detail"))}]}
     :buttons [{:text (localization/message "updater.dialog.button.not-now")
                :cancel-button true
                :result :cancel}
               {:text (localization/message "updater.dialog.button.install-and-restart")
                :result :restart}
               {:text (localization/message "updater.dialog.button.download-newer-version")
                :result :download}]}))

(defn make-platform-no-longer-supported-dialog [^Stage owner localization]
  (make-confirmation-dialog
    localization
    {:title (localization/message "updater.platform-not-supported-dialog.title")
     :icon :icon/circle-sad
     :owner owner
     :header {:fx/type fx.v-box/lifecycle
              :children [{:fx/type fxui/legacy-label
                          :variant :header
                          :text (localization (localization/message "updater.platform-not-supported-dialog.header"))}
                         {:fx/type fxui/legacy-label
                          :text (localization (localization/message "updater.platform-not-supported-dialog.detail"))}]}
     :buttons [{:text (localization/message "dialog.button.close")
                :cancel-button true
                :default-button true}]}))

(defn make-download-update-dialog [^Stage owner localization]
  (make-confirmation-dialog
    localization
    {:title (localization/message "updater.download-dialog.title")
     :header (localization/message "updater.download-dialog.header")
     :icon :icon/circle-happy
     :owner owner
     :buttons [{:text (localization/message "updater.dialog.button.not-now")
                :cancel-button true
                :result false}
               {:text (localization/message "updater.dialog.button.download")
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

(defn- unexpected-error-dialog [{:keys [ex-map localization] :as props}]
  {:fx/type dialog-stage
   :showing (fxui/dialog-showing? props)
   :on-close-request {:result false}
   :title (localization (localization/message "dialog.error.title"))
   :header {:fx/type fx.h-box/lifecycle
            :style-class "spacing-smaller"
            :alignment :center-left
            :children [{:fx/type fxui/icon
                        :type :icon/triangle-sad}
                       {:fx/type fxui/legacy-label
                        :variant :header
                        :text (localization (localization/message "dialog.error.header"))}]}
   :content {:fx/type content-text-area
             :text (messages ex-map)}
   :footer {:fx/type fx.v-box/lifecycle
            :style-class "spacing-smaller"
            :children [{:fx/type fxui/legacy-label
                        :text (localization (localization/message "dialog.error.footer"))}
                       {:fx/type dialog-buttons
                        :children [{:fx/type fxui/legacy-button
                                    :cancel-button true
                                    :on-action {:result false}
                                    :text (localization (localization/message "dialog.button.dismiss"))}
                                   {:fx/type fxui/ext-focused-by-default
                                    :desc {:fx/type fxui/legacy-button
                                           :variant :primary
                                           :default-button true
                                           :on-action {:result true}
                                           :text (localization (localization/message "dialog.button.report"))}}]}]}})

(defn make-unexpected-error-dialog [ex-map sentry-id-promise localization]
  (when (fxui/show-dialog-and-await-result!
          :event-handler (fn [state event]
                           (assoc state ::fxui/result (:result event)))
          :description {:fx/type unexpected-error-dialog
                        :localization localization
                        :ex-map ex-map})
    (let [sentry-id (deref sentry-id-promise 100 nil)
          fields (if sentry-id
                   {"Error" (format "<a href='https://sentry.io/organizations/defold/issues/?query=id%%3A\"%s\"'>%s</a>"
                                    sentry-id sentry-id)}
                   {})]
      (ui/open-url (github/new-issue-link fields)))))

(defn- load-project-dialog [{:keys [progress localization] :as props}]
  {:fx/type dialog-stage
   :showing (fxui/dialog-showing? props)
   :title (ui/make-title)
   :on-close-request (fn [_] (Platform/exit))
   :header {:fx/type fx.h-box/lifecycle
            :style-class "spacing-default"
            :alignment :center-left
            :children [{:fx/type fx.group/lifecycle
                        :children [{:fx/type fx.image-view/lifecycle
                                    :scale-x 0.25
                                    :scale-y 0.25
                                    :image "logo.png"}]}
                       {:fx/type fxui/legacy-label
                        :variant :header
                        :text (localization (localization/message "dialog.loading-project.header"))}]}
   :content {:fx/type fx.v-box/lifecycle
             :style-class ["dialog-content-padding" "spacing-smaller"]
             :children [{:fx/type fxui/legacy-label
                         :wrap-text false
                         :text (localization (progress/message progress))}
                        {:fx/type fx.progress-bar/lifecycle
                         :max-width Double/MAX_VALUE
                         :progress (or (progress/fraction progress)
                                       -1.0)}]} ; Indeterminate.
   :footer {:fx/type dialog-buttons
            :children [{:fx/type fxui/legacy-button
                        :disable true
                        :text (localization (localization/message "dialog.button.cancel"))}]}})

(defn make-load-project-dialog [localization worker-fn]
  (ui/run-now
    (let [state-atom (atom {:progress (progress/make (localization/message "progress.loading") 1 0)})
          renderer (fx/create-renderer
                     :error-handler error-reporting/report-exception!
                     :middleware (fx/wrap-map-desc assoc :fx/type load-project-dialog :localization localization))
          render-progress! #(swap! state-atom assoc :progress %)
          _ (future
              (try
                (swap! state-atom assoc ::fxui/result (worker-fn render-progress!))
                (catch Throwable e
                  (log/error :exception e)
                  (swap! state-atom assoc ::fxui/result e))))
          ret (fxui/mount-renderer-and-await-result! state-atom renderer)]
      (if (instance? Throwable ret)
        (throw ret)
        ret))))

(defn make-gl-support-error-dialog [support-error localization]
  (make-confirmation-dialog
    localization
    {:title (localization/message "dialog.gl-support.title")
     :header {:fx/type fx.v-box/lifecycle
              :children
              (-> [{:fx/type fxui/legacy-label
                    :variant :header
                    :text (localization (localization/message "dialog.gl-support.header"))}]
                  (cond->
                    (os/is-linux?)
                    (conj {:fx/type fx.hyperlink/lifecycle
                           :text (localization (localization/message "dialog.gl-support.linux"))
                           :on-action (fn [_] (ui/open-url "https://defold.com/faq/faq/#linux-questions"))}))
                  (conj
                    {:fx/type fx.hyperlink/lifecycle
                     :on-action (fn [_] (ui/open-url (github/glgenbuffers-link)))
                     :text (localization (localization/message "dialog.gl-support.gl-gen-buffers"))}
                    {:fx/type fxui/legacy-label
                     :text (localization (localization/message "dialog.gl-support.detail"))}))}
     :icon :icon/circle-sad
     :content {:fx/type content-text-area
               :text support-error}
     :buttons [{:text (localization/message "dialog.button.quit")
                :cancel-button true
                :result :quit}
               {:text (localization/message "dialog.button.disable-scene-editor")
                :default-button true
                :result :continue}]}))

(defn make-file-dialog
  ^File [^String title filter-descs ^File initial-file ^Window owner-window]
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

(defn make-directory-dialog
  ^File [^String title ^File initial-dir ^Window owner-window]
  (-> (doto (DirectoryChooser.)
        (.setInitialDirectory initial-dir)
        (.setTitle title))
      (.showDialog owner-window)))

(defn- default-filter-fn [filter-on text items localization]
  (if (coll/empty? text)
    items
    (let [text (string/lower-case text)]
      (filterv (fn [item]
                 (thread-util/throw-if-interrupted!)
                 (string/starts-with? (string/lower-case (filter-on item localization)) text))
               items))))

(def ext-with-identity-items-props
  (fx/make-ext-with-props
    {:items (fx.prop/make (fxui/identity-aware-observable-list-mutator #(.getItems ^ListView %))
                          fx.lifecycle/scalar)}))

(defn- select-first-list-item-on-items-changed! [^ListView view]
  (let [items (.getItems view)
        properties (.getProperties view)
        selection-model (.getSelectionModel view)]
    (when (pos? (count items))
      (.select selection-model 0))
    (.addListener items (reify ListChangeListener
                          (onChanged [_ _]
                            (when (pos? (count items))
                              (.select selection-model 0)
                              ;; ListViewBehavior's selectedIndicesListener sets the selection anchor
                              ;; to the end of the list on items change unless the list view has default anchor.
                              ;; This keeps the anchor at the top so shift+click selects from the start instead of the end:
                              (.put properties "isDefaultAnchor" true)))))))

(defn- select-list-dialog
  [{:keys [filter-term filtered-items filter-in-progress title ok-label prompt cell-fn selection owner localization]
    :as props}]
  {:fx/type dialog-stage
   :title (localization title)
   :showing (fxui/dialog-showing? props)
   :owner owner
   :on-close-request {:event-type :cancel}
   :size :large
   :header {:fx/type fxui/legacy-text-field
            :prompt-text (localization prompt)
            :text filter-term
            :on-text-changed {:event-type :set-filter-term}}
   :root-props {:event-filter {:event-type :filter-root-events}}
   :content {:fx/type fx.ext.list-view/with-selection-props
             :props {:selection-mode selection
                     :on-selected-indices-changed {:event-type :set-selected-indices}}
             :desc {:fx/type fx/ext-on-instance-lifecycle
                    :on-created select-first-list-item-on-items-changed!
                    :desc {:fx/type ext-with-identity-items-props
                           :props {:items filtered-items}
                           :desc {:fx/type fx.list-view/lifecycle
                                  :fixed-cell-size 27
                                  :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                                 :describe cell-fn}}}}}
   :footer {:fx/type fx.h-box/lifecycle
            :alignment :center-left
            :children [{:fx/type fx.progress-indicator/lifecycle
                        :h-box/hgrow :never
                        :pref-width 32.0
                        :pref-height 32.0
                        :visible filter-in-progress}
                       {:fx/type dialog-buttons
                        :h-box/hgrow :always
                        :children [{:fx/type fxui/legacy-button
                                    :text (localization ok-label)
                                    :variant :primary
                                    :disable (zero? (count filtered-items))
                                    :on-action {:event-type :confirm}
                                    :default-button true}]}]}})

(defn- wrap-cell-fn [f localization]
  (fn [item]
    (when (some? item)
      (assoc (f item localization) :on-mouse-clicked {:event-type :select-item-on-double-click}))))

(defn- select-list-dialog-event-handler [set-filter-term-fn]
  (fn [state event]
    (case (:event-type event)
      :cancel (assoc state ::fxui/result nil)
      :confirm (assoc state ::fxui/result {:filter-term (:filter-term state)
                                           :selected-items (mapv (:filtered-items state) (:selected-indices state))})
      :set-selected-indices (assoc state :selected-indices (:fx/event event))
      :select-item-on-double-click (let [^MouseEvent e (:fx/event event)]
                                     (if (and (= MouseButton/PRIMARY (.getButton e))
                                              (= 2 (.getClickCount e)))
                                       (recur state (assoc event :event-type :confirm))
                                       state))
      :filter-root-events (let [^Event e (:fx/event event)]
                            (if (and (instance? KeyEvent e)
                                     (= KeyEvent/KEY_PRESSED (.getEventType e)))
                              (let [^KeyEvent e e]
                                (condp = (.getCode e)
                                  KeyCode/UP (let [view (.getTarget e)]
                                               (when (instance? ListView (.getTarget e))
                                                 (let [^ListView view view]
                                                   (when (zero? (.getSelectedIndex (.getSelectionModel view)))
                                                     (.consume e)
                                                     (.fireEvent view (KeyEvent.
                                                                        KeyEvent/KEY_PRESSED
                                                                        "\t"
                                                                        "\t"
                                                                        KeyCode/TAB
                                                                        #_shift true
                                                                        #_control false
                                                                        #_alt false
                                                                        #_meta false)))))
                                               state)
                                  KeyCode/DOWN (let [view (.getTarget e)]
                                                 (when (instance? TextField view)
                                                   (let [^TextField view view]
                                                     (.consume e)
                                                     (.fireEvent view (KeyEvent.
                                                                        KeyEvent/KEY_PRESSED
                                                                        "\t"
                                                                        "\t"
                                                                        KeyCode/TAB
                                                                        #_shift false
                                                                        #_control false
                                                                        #_alt false
                                                                        #_meta false))))
                                                 state)
                                  KeyCode/ENTER (if (empty? (:filtered-items state))
                                                  state
                                                  (recur state (assoc event :event-type :confirm)))
                                  KeyCode/ESCAPE (recur state (assoc event :event-type :cancel))
                                  state))
                              state))
      :set-filter-term (let [filter-term (:fx/event event)]
                         (set-filter-term-fn state filter-term)))))

(defn- default-cell-fn [item _localization]
  item)

(defn- default-filter-on [item _localization]
  (str item))

(defn make-select-list-dialog
  "Show dialog that allows the user to select one or many of the suggested items

  Returns a coll of selected items or nil if nothing was selected

  Supported options keys (all optional):

      :title          dialog title, string or MessagePattern, defaults to
                      \"dialog.select-item.title\" message
      :ok-label       label on confirmation button, string or MessagePattern,
                      defaults to \"dialog.select-item.button.ok\" message
      :filter         initial filter term string, defaults to value in
                      :filter-atom, or, if absent, to empty string
      :filter-atom    atom with initial filter term string; if supplied, its
                      value will be reset to final filter term on confirm
      :cell-fn        cell factory fn, will receive 2 args: an item and
                      localization; should return a cljfx prop map for a list
                      cell; returns the item by default
      :selection      a selection mode, either :single (default) or :multiple
      :filter-fn      filtering fn of 2 args (filter term and items), should
                      return a filtered coll of items
      :filter-on      if no custom :filter-fn is supplied, will receive 2 args:
                      item and localization; use this fn of item to string for
                      default filtering; stringifies item by default
      :prompt         filter text field's prompt, string or MessagePattern,
                      defaults to \"dialog.select-item.prompt\" message
      :owner          the owner window, defaults to main stage"
  ([items localization]
   (make-select-list-dialog items localization {}))
  ([items localization options]
   (let [items (localization/sort-if-annotated @localization items)
         cell-fn (wrap-cell-fn (:cell-fn options default-cell-fn) localization)
         filter-atom (:filter-atom options)
         filter-fn (or (:filter-fn options)
                       (fn [text items]
                         (default-filter-fn (:filter-on options default-filter-on) text items localization)))
         filter-term (or (:filter options)
                         (some-> filter-atom deref)
                         "")
         filter-future-atom (atom nil)
         state-atom (atom {:filter-term nil ; Non-string value to ensure set-filter-term won't early-out.
                           :filter-in-progress false
                           :filtered-items []
                           :selected-indices []})

         set-filter-term
         (fn set-filter-term [state filter-term]
           (if (= (:filter-term state) filter-term)
             state
             (do (swap! filter-future-atom
                        (fn [pending-filter-future]
                          (thread-util/cancel-future! pending-filter-future)
                          (future
                            (try
                              (let [filtered-items (vec (filter-fn filter-term items))
                                    selected-indices (if (coll/empty? filtered-items) [] [0])]
                                (thread-util/throw-if-interrupted!)
                                (swap! state-atom
                                       (fn [state]
                                         (thread-util/throw-if-interrupted!)
                                         (assoc state
                                           :filter-in-progress false
                                           :filtered-items filtered-items
                                           :selected-indices selected-indices))))
                              (catch InterruptedException _
                                nil)
                              (catch Throwable error
                                (error-reporting/report-exception! error))))))
                 (assoc state
                   :filter-term filter-term
                   :filter-in-progress true))))

         _ (swap! state-atom set-filter-term filter-term)
         event-handler (select-list-dialog-event-handler set-filter-term)
         result (fxui/show-dialog-and-await-result!
                  :state-atom state-atom
                  :event-handler event-handler
                  :description {:fx/type select-list-dialog
                                :localization localization
                                :title (or (:title options)
                                           (localization/message "dialog.select-item.title"))
                                :ok-label (or (:ok-label options)
                                              (localization/message "dialog.select-item.button.ok"))
                                :prompt (or (:prompt options)
                                            (localization/message "dialog.select-item.prompt"))
                                :cell-fn cell-fn
                                :owner (or (:owner options) (ui/main-stage))
                                :selection (:selection options :single)})]
     (swap! filter-future-atom thread-util/cancel-future!)
     (when result
       (let [{:keys [filter-term selected-items]} result]
         (when (and filter-atom selected-items)
           (reset! filter-atom filter-term))
         selected-items)))))

(declare sanitize-folder-name)

(defn- new-folder-dialog
  [{:keys [name validate localization] :as props}]
  (let [sanitized-name ^String (sanitize-folder-name name)
        path-empty (zero? (.length sanitized-name))
        error-msg (validate sanitized-name)
        invalid (boolean (seq error-msg))]
    {:fx/type dialog-stage
     :showing (fxui/dialog-showing? props)
     :on-close-request {:event-type :cancel}
     :title (localization (localization/message "dialog.new-folder.title"))
     :size :small
     :header {:fx/type fxui/legacy-label
              :variant :header
              :text (localization (localization/message "dialog.new-folder.header"))}
     :content {:fx/type fxui/two-col-input-grid-pane
               :style-class "dialog-content-padding"
               :children [{:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.new-folder.label.name"))}
                          {:fx/type fxui/legacy-text-field
                           :text ""
                           :variant (if invalid :error :default)
                           :on-text-changed {:event-type :set-folder-name}}
                          {:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.new-folder.label.preview"))}
                          {:fx/type fxui/legacy-text-field
                           :editable false
                           :text (or error-msg sanitized-name)}]}
     :footer {:fx/type dialog-buttons
              :children [{:fx/type fxui/legacy-button
                          :text (localization (localization/message "dialog.button.cancel"))
                          :cancel-button true
                          :on-action {:event-type :cancel}}
                         {:fx/type fxui/legacy-button
                          :disable (or invalid path-empty)
                          :text (localization (localization/message "dialog.new-folder.button.create-folder"))
                          :variant :primary
                          :default-button true
                          :on-action {:event-type :confirm}}]}}))

(defn make-new-folder-dialog
  [{:keys [validate localization]}]
  {:pre [(some? localization)]}
  (fxui/show-dialog-and-await-result!
    :initial-state {:name ""
                    :validate (or validate (constantly nil))}
    :event-handler (fn [state {:keys [fx/event event-type]}]
                     (case event-type
                       :set-folder-name (assoc state :name event)
                       :cancel (assoc state ::fxui/result nil)
                       :confirm (assoc state ::fxui/result (sanitize-folder-name (:name state)))))
    :description {:fx/type new-folder-dialog
                  :localization localization}))

(defn- target-ip-dialog [{:keys [msg ^String ip localization] :as props}]
  (let [ip-valid (pos? (.length ip))]
    {:fx/type dialog-stage
     :showing (fxui/dialog-showing? props)
     :on-close-request {:event-type :cancel}
     :title (localization (localization/message "dialog.target-ip.title"))
     :size :small
     :header {:fx/type fxui/legacy-label
              :variant :header
              :text msg}
     :content {:fx/type fxui/two-col-input-grid-pane
               :style-class "dialog-content-padding"
               :children [{:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.target-ip.label.ip"))}
                          {:fx/type fxui/legacy-text-field
                           :variant (if ip-valid :default :error)
                           :text ip
                           :on-text-changed {:event-type :set-ip}}]}
     :footer {:fx/type dialog-buttons
              :children [{:fx/type fxui/legacy-button
                          :text (localization (localization/message "dialog.button.cancel"))
                          :cancel-button true
                          :on-action {:event-type :cancel}}
                         {:fx/type fxui/legacy-button
                          :disable (not ip-valid)
                          :text (localization (localization/message "dialog.target-ip.button.add"))
                          :variant :primary
                          :default-button true
                          :on-action {:event-type :confirm}}]}}))

(defn make-target-ip-dialog [ip msg localization]
  (fxui/show-dialog-and-await-result!
    :initial-state {:ip (or ip "")}
    :event-handler (fn [state {:keys [fx/event event-type]}]
                     (case event-type
                       :set-ip (assoc state :ip event)
                       :cancel (assoc state ::fxui/result nil)
                       :confirm (assoc state ::fxui/result (:ip state))))
    :description {:fx/type target-ip-dialog
                  :localization localization
                  :msg (or msg (localization (localization/message "dialog.target-ip.header")))}))

(defn- sanitize-common [name]
  (-> name
      (string/replace #"[/\\]" "") ; strip path separators
      (string/replace #"[\"'«»]" "") ; strip quotes
      (string/replace #"[<>:|?*]" "") ; Additional Windows forbidden characters
      string/trim))

(defn sanitize-file-name [extension name]
  (let [name (sanitize-common name)]
    (cond-> name
            ; disallow "." and ".." names (only necessary when there is no extension)
            (not (seq extension)) (string/replace #"^\.{1,2}$" "")
            ; append extension if there was one
            (and (seq extension) (seq name)) (str "." extension))))

(defn- apply-extension [name extension]
  (cond-> name (seq extension) (str "." extension)))

(defn- sanitize-against-extensions
  "Sanitizes the file/folder name against a coll of possible extensions

  Returns a valid (and possibly empty!), file/folder name, or nil"
  [name extensions]
  {:pre [(string? name) (seq extensions)]}
  (let [name (sanitize-common name)]
    (when (every? (fn [extension]
                    (let [full-name (apply-extension name extension)]
                      (and (pos? (count full-name))
                           (not= "." full-name)
                           (not= ".." full-name))))
                  extensions)
      name)))

(defn sanitize-folder-name [name]
  (sanitize-common name))

(defn sanitize-path [path]
  (string/join \/
               (into []
                     (comp
                       (remove empty?)
                       (map sanitize-folder-name))
                     (string/split path #"[\\\/]"))))

(defn- rename-dialog [{:keys [initial-name name title label extensions validate localization] :as props}]
  (let [sanitized (sanitize-against-extensions name extensions)
        validation-msg (when sanitized
                         (some #(validate (apply-extension sanitized %)) extensions))
        invalid (or (not sanitized) (some? validation-msg))]
    {:fx/type dialog-stage
     :showing (fxui/dialog-showing? props)
     :on-close-request {:event-type :cancel}
     :title (localization title)
     :size :small
     :header {:fx/type fxui/legacy-label
              :variant :header
              :text (localization (localization/message "dialog.rename.title" {"name" initial-name}))}
     :content {:fx/type fxui/two-col-input-grid-pane
               :style-class "dialog-content-padding"
               :children [{:fx/type fx.label/lifecycle
                           :text (localization label)}
                          {:fx/type fxui/legacy-text-field
                           :text name
                           :variant (if invalid :error :default)
                           :on-text-changed {:event-type :set-name}}
                          {:fx/type fx.label/lifecycle
                           :text (localization (localization/message "dialog.rename.preview"))}
                          {:fx/type fxui/legacy-text-field
                           :editable false
                           :text (or (some-> validation-msg localization)
                                     (->> extensions
                                          (map #(apply-extension sanitized %))
                                          (string/join ", ")))}]}
     :footer {:fx/type dialog-buttons
              :children [{:fx/type fxui/legacy-button
                          :text (localization (localization/message "dialog.button.cancel"))
                          :cancel-button true
                          :on-action {:event-type :cancel}}
                         {:fx/type fxui/legacy-button
                          :variant :primary
                          :default-button true
                          :disable invalid
                          :text (localization (localization/message "dialog.rename.button.rename"))
                          :on-action {:event-type :confirm}}]}}))

(defn make-rename-dialog
  "Shows rename dialog

  Returns either file name that fits all extensions (might be empty!) or nil

  Options expect kv-args:
    :localization    a Localization instance
    :title           dialog title, a MessagePattern
    :label           name input label, a MessagePattern
    :extensions      non-empty coll of used extension for renamed file(s), where
                     empty string or nil item means the renamed file will be
                     used without any extensions
    :validate        1-arg fn from a sanitized file name to either nil (if
                     valid) or MessagePattern (error message)"
  ^String [name & {:as options}]
  (fxui/show-dialog-and-await-result!
    :initial-state {:name name}
    :event-handler (fn [state event]
                     (case (:event-type event)
                       :set-name (assoc state :name (:fx/event event))
                       :cancel (assoc state ::fxui/result nil)
                       :confirm (assoc state ::fxui/result
                                             (sanitize-against-extensions
                                               (:name state)
                                               (:extensions options)))))
    :description (assoc options :fx/type rename-dialog
                                :localization (:localization options)
                                :initial-name name)))

(defn- relativize [^File base ^File path]
  (let [[^Path base ^Path path] (map #(Paths/get (.toURI ^File %)) [base path])]
    (str ""
         (when (.startsWith path base)
           (-> base
             (.relativize path)
             (.toString))))))

(defn- new-file-dialog
  [{:keys [^File base-dir ^File location type ext name localization] :as props}]
  (let [sanitized-name (sanitize-file-name ext name)
        empty (empty? sanitized-name)
        relative-path (FilenameUtils/separatorsToUnix (relativize base-dir location))
        location-exists (.exists location)
        valid-input (and (not empty) location-exists)]
    {:fx/type dialog-stage
     :showing (fxui/dialog-showing? props)
     :on-close-request {:event-type :cancel}
     :title (if type
              (localization (localization/message "dialog.new-file.title.resource" {"type" type}))
              (localization (localization/message "dialog.new-file.title.file")))
     :size :small
     :header {:fx/type fxui/legacy-label
              :variant :header
              :text (if type
                      (localization (localization/message "dialog.new-file.header.resource" {"type" type}))
                      (localization (localization/message "dialog.new-file.header.file")))}
     :content {:fx/type fxui/two-col-input-grid-pane
               :style-class "dialog-content-padding"
               :children [{:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.new-file.label.name"))}
                          {:fx/type fxui/legacy-text-field
                           :text ""
                           :variant (if empty :error :default)
                           :on-text-changed {:event-type :set-file-name}}
                          {:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.new-file.label.location"))}
                          {:fx/type fx.h-box/lifecycle
                           :spacing 4
                           :children [{:fx/type fxui/legacy-text-field
                                       :h-box/hgrow :always
                                       :variant (if location-exists :default :error)
                                       :on-text-changed {:event-type :set-location}
                                       :text relative-path}
                                      {:fx/type fxui/legacy-button
                                       :variant :icon
                                       :on-action {:event-type :pick-location}
                                       :text "…"}]}
                          {:fx/type fxui/legacy-label
                           :text (localization (localization/message "dialog.new-file.label.preview"))}
                          {:fx/type fxui/legacy-text-field
                           :editable false
                           :text (if valid-input
                                   (str relative-path \/ sanitized-name)
                                   "")}]}
     :footer {:fx/type dialog-buttons
              :children [{:fx/type fxui/legacy-button
                          :text (localization (localization/message "dialog.button.cancel"))
                          :cancel-button true
                          :on-action {:event-type :cancel}}
                         {:fx/type fxui/legacy-button
                          :disable (not valid-input)
                          :text (if type
                                  (localization (localization/message "dialog.new-file.button.create.resource" {"type" type}))
                                  (localization (localization/message "dialog.new-file.button.create.file")))
                          :variant :primary
                          :default-button true
                          :on-action {:event-type :confirm}}]}}))

(defn make-new-file-dialog
  [^File base-dir ^File location type ext localization]
  (fxui/show-dialog-and-await-result!
    :initial-state {:name ""
                    :base-dir base-dir
                    :location location
                    :type type
                    :ext ext}
    :event-handler (fn [state {:keys [fx/event event-type]}]
                     (case event-type
                       :set-file-name (assoc state :name event)
                       :set-location (assoc state :location (io/file base-dir (sanitize-path event)))
                       :pick-location (assoc state :location
                                             (let [window (fxui/event->window event)
                                                   previous-location (:location state)
                                                   initial-dir (if (.exists ^File previous-location)
                                                                 previous-location
                                                                 base-dir)
                                                   path (make-directory-dialog (localization (localization/message "dialog.directory.title.set-path")) initial-dir window)]
                                               (if path
                                                 (io/file base-dir (relativize base-dir path))
                                                 previous-location)))
                       :cancel (assoc state ::fxui/result nil)
                       :confirm (assoc state ::fxui/result
                                       (-> (io/file (:location state) (sanitize-file-name ext (:name state)))
                                           ;; Canonical path turns Windows path
                                           ;; into the correct case. We need
                                           ;; this to be able to match internal
                                           ;; resource maps, which are case
                                           ;; sensitive unlike the NTFS file
                                           ;; system.
                                           (.getCanonicalFile)))))
    :description {:fx/type new-file-dialog
                  :localization localization}))

(defn ^:dynamic make-resolve-file-conflicts-dialog
  [src-dest-pairs localization]
  ;; We do not allow the user to choose the :overwrite action if we're about to
  ;; overwrite a broken symlink.
  (let [any-dest-is-broken-symlink
        (coll/any? (fn [[_ ^File dest]]
                     (and (not (.exists dest))
                          (path/symlink? dest)))
                   src-dest-pairs)]

    (make-confirmation-dialog
      localization
      {:icon :icon/circle-question
       :size :large
       :title (localization/message "dialog.name-conflict.title")
       :header (localization/message "dialog.name-conflict.header" {"count" (count src-dest-pairs)})
       :buttons (cond-> [{:text (localization/message "dialog.button.cancel")
                          :cancel-button true
                          :default-button true}
                         {:text (localization/message "dialog.name-conflict.button.name-differently")
                          :result :rename}]

                        (not any-dest-is-broken-symlink)
                        (conj {:text (localization/message "dialog.name-conflict.button.overwrite-files")
                               :variant :danger
                               :result :overwrite}))})))

(def ext-with-selection-props
  (fx/make-ext-with-props
    {:selection fxui/text-input-selection-prop}))

(defn make-target-log-dialog [log-atom clear! restart! localization]
  (let [renderer-ref (volatile! nil)
        renderer (fx/create-renderer
                   :error-handler error-reporting/report-exception!
                   :opts {:fx.opt/map-event-handler
                          #(case (:event-type %)
                             :clear (clear!)
                             :restart (restart!)
                             :cancel (fx/unmount-renderer log-atom @renderer-ref))}
                   :middleware
                   (fx/wrap-map-desc
                     (fn [log]
                       {:fx/type dialog-stage
                        :title (localization (localization/message "dialog.target-discovery-log.title"))
                        :modality :none
                        :showing true
                        :size :large
                        :header {:fx/type fxui/legacy-label
                                 :variant :header
                                 :text (localization (localization/message "dialog.target-discovery-log.header"))}
                        :content (let [str (string/join "\n" log)]
                                   {:fx/type ext-with-selection-props
                                    :props {:selection [(count str) (count str)]}
                                    :desc {:fx/type content-text-area
                                           :pref-row-count 20
                                           :text str}})
                        :footer {:fx/type dialog-buttons
                                 :children [{:fx/type fxui/legacy-button
                                             :text (localization (localization/message "dialog.button.close"))
                                             :cancel-button true
                                             :on-action {:event-type :cancel}}
                                            {:fx/type fxui/legacy-button
                                             :text (localization (localization/message "dialog.target-discovery-log.button.clear-log"))
                                             :on-action {:event-type :clear}}
                                            {:fx/type fxui/legacy-button
                                             :text (localization (localization/message "dialog.target-discovery-log.button.restart-discovery"))
                                             :on-action {:event-type :restart}}]}})))]
    (vreset! renderer-ref renderer)
    (fx/mount-renderer log-atom renderer)))

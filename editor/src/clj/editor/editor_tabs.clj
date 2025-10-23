;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.editor-tabs
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.icons :as icons]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.recent-files :as recent-files]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view])
  (:import [java.util Arrays Collection List]
           [javafx.collections ObservableList]
           [javafx.scene Node]
           [javafx.scene.layout AnchorPane HBox Priority Region VBox]
           [javafx.scene.control Hyperlink Label SplitPane Tab TabPane Tooltip]))

(defn- make-info-box! [localization]
  (let [info-panel (HBox.)
        left-label (doto (Label.) (localization/localize! localization (localization/message "resource.readonly.detail")))
        right-link (doto (Hyperlink.) (localization/localize! localization (localization/message "resource.readonly.button.read-more")))
        spacer (Region.)]
    (HBox/setHgrow spacer Priority/ALWAYS)
    (.getStyleClass info-panel)
    (ui/set-style! info-panel "info-panel" true)
    (ui/set-style! left-label "info-panel-label" true)
    (ui/set-style! right-link "info-panel-link" true)
    (.setOnAction right-link (ui/event-handler _ (ui/open-url "https://defold.com/manuals/libraries/#editing-files-in-library-dependencies")))
    (.addAll  (.getChildren info-panel) (Arrays/asList (into-array Node [left-label spacer right-link])))
    info-panel))

(defn tab-title
  ^String [resource dirty]
  ;; Lone underscores are treated as mnemonic letter signifiers in the overflow
  ;; dropdown menu, and we cannot disable mnemonic parsing for it since the
  ;; control is internal. We also cannot replace them with double underscores to
  ;; escape them, as they will show up in the Tab labels and there appears to be
  ;; no way to enable mnemonic parsing for them. Attempts were made to call
  ;; setMnemonicParsing on the parent Labelled as the Tab graphic was added to
  ;; the DOM, but this only worked on macOS. As a workaround, we instead replace
  ;; underscores with the a unicode character that looks somewhat similar.
  (let [resource-name (resource/resource-name resource)
        escaped-resource-name (string/replace resource-name "_" "\u02CD")]
    (if dirty
      (str "*" escaped-resource-name)
      escaped-resource-name)))

(defn make-tab! [app-view prefs localization workspace project resource resource-node
                  resource-type view-type ^ObservableList tabs make-view-fn
                  open-resource-fn select-fn opts]
  (let [parent (AnchorPane.)
        tab-content (if (resource/read-only? resource)
                      (doto (VBox.)
                        (ui/children! [(make-info-box! localization)
                                       (doto parent (VBox/setVgrow Priority/ALWAYS))]))
                      parent)
        tab (doto (Tab. (tab-title resource false))
              (.setContent tab-content)
              (.setTooltip (Tooltip. (or (resource/proj-path resource) "unknown")))
              (ui/user-data! :editor.app-view/view-type view-type))
        view-graph (g/make-graph! :history false :volatility 2)
        select-fn2 (partial select-fn app-view)
        opts (merge opts
                    (get (:view-opts resource-type) (:id view-type))
                    {:app-view app-view
                     :select-fn select-fn2
                     :open-resource-fn (partial open-resource-fn app-view prefs localization workspace project)
                     :prefs prefs
                     :project project
                     :workspace workspace
                     :localization localization
                     :tab tab})
        view (make-view-fn view-graph parent resource-node opts)]
    (assert (g/node-instance? view/WorkbenchView view))
    (recent-files/add! prefs resource view-type)
    (g/transact
      (concat
        (view/connect-resource-node view resource-node)
        (g/connect view :view-data app-view :open-views)
        (g/connect view :view-dirty app-view :open-dirty-views)))
    (ui/user-data! tab :editor.app-view/view view)
    (.add tabs tab)
    (.setGraphic tab (icons/get-image-view (or (:icon resource-type) "icons/64/Icons_29-AT-Unknown.png") 16))
    (.addAll (.getStyleClass tab) ^Collection (resource/style-classes resource))
    (ui/register-tab-toolbar tab "#toolbar" :toolbar)
    (.setOnSelectionChanged tab (ui/event-handler event
                                  (when (.isSelected tab)
                                    (recent-files/add! prefs resource view-type))))
    (let [close-handler (.getOnClosed tab)]
      (.setOnClosed tab (ui/event-handler event
                          (recent-files/add! prefs resource view-type)
                          ;; The menu refresh can occur after the view graph is
                          ;; deleted but before the tab controls lose input
                          ;; focus, causing handlers to evaluate against deleted
                          ;; graph nodes. Using run-later here prevents this.
                          (ui/run-later
                            (doto tab
                              (ui/user-data! :editor.app-view/view-type nil)
                              (ui/user-data! :editor.app-view/view nil))
                            (g/delete-graph! view-graph))
                          (when close-handler
                            (.handle close-handler event)))))
    tab))

(defn- tab->resource [^Tab tab]
  (some-> tab
    (ui/user-data :editor.app-view/view)
    (g/node-value :view-data)
    second
    :resource))

(defn- serialize-open-tabs [app-view]
  (let [editor-tabs-split ^SplitPane (g/with-auto-evaluation-context ec
                                       (g/node-value app-view :editor-tabs-split ec))]
    (mapv (fn [^TabPane tab-pane]
            (mapv (fn [tab]
                    [(resource/proj-path (tab->resource tab))
                     (-> tab
                         tab->resource
                         resource/resource-type
                         :view-types
                         first
                         :id)])
                  (.getTabs tab-pane)))
          (.getItems editor-tabs-split))))

(defn- serialize-tab-selections [app-view]
  (g/with-auto-evaluation-context evaluation-context
    (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split evaluation-context)
          active-tab-pane (g/node-value app-view :active-tab-pane evaluation-context)
          tab-panes (.getItems editor-tabs-split)]
      {:selected-pane (.indexOf tab-panes active-tab-pane)
       :tab-selection-by-pane (mapv (fn [^TabPane pane]
                                  (-> pane .getSelectionModel .getSelectedIndex))
                                tab-panes)})))

(defn save-open-tabs [prefs app-view]
  (prefs/set! prefs [:workflow :open-tabs] (serialize-open-tabs app-view)))

(defn save-tab-selections [prefs app-view]
  (prefs/set! prefs [:workflow :last-selected-tabs] (serialize-tab-selections app-view)))

(comment
  (defn save-open-tabs [prefs app-view] nil)
  (defn save-tab-selections [prefs app-view] nil)
  (prefs/set! (dev/prefs) [:workflow :open-tabs] [[["/main/main.collection" :collection] ["/scripts/knight.script" :code]]
                                                  [["/scripts/utils_blah.lua" :code]["/scripts/utils.lua" :code]]])
  (prefs/set! (dev/prefs) [:workflow :open-tabs] [[["/scripts/utils.lua" :code]["/scripts/knight.script" :code]]])
  (prefs/set! (dev/prefs) [:workflow :last-selected-tabs] {:selected-pane 1, :tab-selection-by-pane [0 0]})
  (prefs/get (dev/prefs) [:workflow :open-tabs])
  (prefs/get (dev/prefs) [:workflow :last-selected-tabs])
  ,)

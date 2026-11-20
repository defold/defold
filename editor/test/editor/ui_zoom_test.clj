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

(ns editor.ui-zoom-test
  (:require [cljfx.api :as fx]
            [clojure.test :refer [deftest is testing use-fixtures]]
            [editor.fs :as fs]
            [editor.fxui :as fxui]
            [editor.prefs :as prefs]
            [editor.ui :as ui]
            [editor.ui-zoom :as ui-zoom]
            [support.test-support :as test-support])
  (:import [com.defold.control ZoomPane]
           [javafx.scene Node Scene]
           [javafx.scene.control Button ContextMenu MenuItem]
           [javafx.scene.layout StackPane VBox]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- with-ui-scale [f]
  (test-support/with-cleared-system-properties! ["defold.uiScale"]
    (let [preferences (prefs/global (fs/create-temp-file! "ui-zoom" ".edn"))]
      (prefs/set! preferences [:window :ui-scale] 150)
      (ui-zoom/initialize! preferences)
      (try
        (f)
        (finally
          (prefs/reset-path! preferences [:window :ui-scale])
          (ui-zoom/initialize! preferences))))))

(use-fixtures :once with-ui-scale)

;; Invalid startup overrides must fall back to the saved preference.
(deftest startup-scale-test
  (is (= 1.25 (ui-zoom/startup-scale 1.25 nil)))
  (is (= 1.5 (ui-zoom/startup-scale 1.25 "1.5")))
  (doseq [override ["" "abc" "NaN" "Infinity" "0" "-1" "0.5" "3.1"]]
    (is (= 1.25 (ui-zoom/startup-scale 1.25 override))))
  (is (prefs/valid?
        (get-in prefs/default-schema [:properties :window :properties :ui-scale])
        150)))

;; Zoom fills the available area after resizing and preserves input coordinates.
(deftest zoom-pane-layout-test
  (ui/run-now
    (let [content (doto (StackPane.) (.setPrefSize 200.0 100.0))
          pane (doto (ZoomPane. 1.5) (.setContent content))]
      (is (= 300.0 (.prefWidth pane -1.0)))
      (is (= 150.0 (.prefHeight pane -1.0)))
      (doseq [[width height] [[600.0 300.0] [900.0 600.0]]]
        (.resize pane width height)
        (.layout pane)
        (is (= (/ (double width) 1.5) (.getWidth content)))
        (is (= (/ (double height) 1.5) (.getHeight content)))
        (is (= width (.getMaxX (.getBoundsInParent content))))
        (is (= height (.getMaxY (.getBoundsInParent content))))
        (is (= 0.0 (.getMinX (.getBoundsInParent content))))
        (let [point (.parentToLocal content 150.0 75.0)]
          (is (= 100.0 (.getX point)))
          (is (= 50.0 (.getY point))))))))

;; Both window creation paths must zoom, including cljfx replacing a scene root.
(deftest stage-zoom-test
  (ui/run-now
    (testing "imperative stage and FXML-style root stylesheets"
      (let [root (doto (VBox.)
                   (-> .getStylesheets (.add "file:/zoom-test.css")))
            stage (ui/make-stage)
            scene (Scene. root)]
        (.setScene stage scene)
        (is (instance? ZoomPane (.getRoot scene)))
        (is (= ["file:/zoom-test.css"] (vec (.getStylesheets (.getRoot scene)))))
        (is (identical? root (.get (.getChildrenUnmodifiable (.getRoot scene)) 0)))))
    (testing "cljfx stage and root replacement"
      (let [desc {:fx/type fxui/stage
                  :scene {:fx/type :scene
                          :root {:fx/type :v-box}}}
            component (volatile! (fx/create-component desc))
            stage ^Stage (fx/instance @component)]
        (try
          (is (instance? ZoomPane (.getRoot (.getScene stage))))
          (vswap! component fx/advance-component (assoc-in desc [:scene :root :fx/type] :stack-pane))
          (let [root (.getRoot (.getScene stage))]
            (is (instance? ZoomPane root))
            (is (= StackPane (class (.get (.getChildrenUnmodifiable root) 0)))))
          (finally
            (fx/delete-component @component)))))))

;; Menus enlarge with their owner without accumulating scale on reopening.
(deftest popup-zoom-test
  (ui/run-now
    (let [button (Button. "Menu")
          content (doto (StackPane.) (-> .getChildren (.add button)))
          stage (doto (ui/make-stage) (.setScene (Scene. content)))
          menu (doto (ContextMenu.) (-> .getItems (.add (MenuItem. "Item"))))]
      (try
        (.show stage)
        (dotimes [_ 2]
          (.show menu button 100.0 100.0)
          (let [root (.getRoot (.getScene menu))
                child ^Node (.get (.getChildrenUnmodifiable root) 0)]
            (is (= 1.5 (.getMxx (.getLocalToParentTransform child)))))
          (.hide menu))
        (finally
          (.hide menu)
          (.hide stage))))))

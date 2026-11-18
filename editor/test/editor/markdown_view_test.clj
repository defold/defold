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

(ns editor.markdown-view-test
  (:require [cljfx.api :as fx]
            [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.markdown-view :as markdown-view]
            [editor.ui :as ui])
  (:import [javafx.scene Scene]
           [javafx.scene.control ScrollPane]))

(defn- with-view [props f]
  (ui/run-now
    (let [component (volatile! (fx/create-component
                                 (assoc props :fx/type markdown-view/html-view)))
          ^ScrollPane pane (fx/instance @component)
          scene (Scene. pane)]
      (.add (.getStylesheets scene) (str (io/resource "editor.css")))
      (try
        (.applyCss pane)
        (f component pane)
        (finally
          (fx/delete-component @component))))))

(defn- layout! [^ScrollPane pane]
  (.applyCss pane)
  (.resize pane 450.0 (min 200.0 (.prefHeight pane 450.0)))
  (.layout pane))

(defn- scroll-code-block-after-update [key-before key-after]
  (let [props {:fx/type markdown-view/html-view
               :html (str "<pre>" (apply str (repeat 120 "x")) "</pre>")
               :max-width 450.0}
        hvalue (volatile! nil)]
    (with-view
      (assoc props :code-block-scroll-key key-before)
      (fn scroll-and-update [component ^ScrollPane pane]
        (layout! pane)
        (.setHvalue ^ScrollPane (.lookup pane ".md-code-scroll-pane") 0.5)
        (vswap! component fx/advance-component (assoc props :code-block-scroll-key key-after))
        (layout! pane)
        (vreset! hvalue (.getHvalue ^ScrollPane (.lookup pane ".md-code-scroll-pane")))))
    @hvalue))

;; Selecting another completion with identical code resets the code block
;; scroll, guarding against the previous selection's scroll position carrying
;; over.
(deftest code-block-scroll-resets-on-selection-change-test
  (is (= 0.0 (scroll-code-block-after-update [0 :first] [1 :second]))))

;; Updating the same selection with unchanged content keeps the code block
;; scroll, guarding against resetting the scroll on every refresh.
(deftest code-block-scroll-preserved-on-same-selection-test
  (is (= 0.5 (scroll-code-block-after-update [0 :first] [0 :first]))))

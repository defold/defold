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

(ns editor.scene-layout-test
  (:require [clojure.test :refer :all]
            [editor.scene-layout :as layout]
            [editor.types :refer [->Region]])
  (:import [editor.types Region]))

;                               left right top bottom
(def ^:private viewport (->Region 0 640 0 480))

(deftest hsplit []
  (let [desc {:type :hbox
              :children [{:id :test1}
                         {:id :test2}]}
        result (layout/layout desc viewport)]
    (is (= (:test1 result) (->Region 0 320 0 480)))
    (is (= (:test2 result) (->Region 320 640 0 480)))))

(deftest anchor []
  (let [desc {:id :parent
              :anchors {:bottom 0}
              :max-height 20
              :children [{:id :child}]}
        result (layout/layout desc viewport)]
    (is (= (:parent result) (->Region 0 640 460 480)))
    (is (= (:child result) (->Region 0 640 460 480)))))

(deftest timeline []
  (let [desc {:anchors {:bottom 0}
              :padding 20
              :max-height 72
              :children [{:id :play
                          :max-width 32}
                         {:id :timeline}]}
        result (layout/layout desc viewport)]
    (is (= (:play result) (->Region 20 52 428 460)))
    (is (= (:timeline result) (->Region 52 620 428 460)))))

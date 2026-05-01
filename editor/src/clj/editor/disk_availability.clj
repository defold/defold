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

(ns editor.disk-availability
  (:require [editor.ui :as ui])
  (:import (javafx.beans.property SimpleBooleanProperty)))

(set! *warn-on-reflection* true)

(def ^:private busy-refcount-atom (atom 0))

(defn available? []
  (zero? @busy-refcount-atom))

(defn push-busy! []
  (swap! busy-refcount-atom inc))

(defn pop-busy! []
  (swap! busy-refcount-atom (fn [busy-refcount]
                              (assert (pos? busy-refcount))
                              (dec busy-refcount))))

;; WARNING:
;; Observing or binding to an observable that lives longer than the observer will
;; cause a memory leak. You must manually unhook them or use weak listeners.
;; Source: https://community.oracle.com/message/10360893#10360893
(defonce ^SimpleBooleanProperty available-property (SimpleBooleanProperty. true))

(add-watch busy-refcount-atom ::busy-refcount-watch
           (fn [_ _ _ _]
             (ui/run-later
               (.set available-property (available?)))))

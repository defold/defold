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

(ns internal.low-memory
  (:require [service.log :as log])
  (:import [java.lang.management ManagementFactory MemoryNotificationInfo MemoryPoolMXBean MemoryType]
           [javax.management NotificationEmitter NotificationListener]))

;; Low memory detection system based on
;; https://www.javaspecialists.eu/archive/Issue092.html

(set! *warn-on-reflection* true)

(def ^:private memory-usage-threshold 0.9)

(def ^:private callback-fns-atom (atom []))

(def ^:private ^NotificationListener notification-listener
  (reify NotificationListener
    (handleNotification [_ notification _]
      (when (= (.getType notification)
               MemoryNotificationInfo/MEMORY_COLLECTION_THRESHOLD_EXCEEDED)
        (doseq [callback-fn @callback-fns-atom]
          (callback-fn))))))

(defn- find-tenured-generation-pool
  ^MemoryPoolMXBean []
  ;; The tenured generation pool is the pool that hosts all the objects that
  ;; survived garbage collection. This is the pool we want to monitor. It is
  ;; the first pool of type HEAP that supports a usage threshold.
  (some (fn [^MemoryPoolMXBean pool]
          (when (and (= MemoryType/HEAP (.getType pool))
                     (.isUsageThresholdSupported pool))
            pool))
        (ManagementFactory/getMemoryPoolMXBeans)))

(def ^:private ensure-notification-listener!
  (memoize
    (fn ensure-notification-listener! []
      (when-some [tenured-generation-pool (find-tenured-generation-pool)]
        (when (.isCollectionUsageThresholdSupported tenured-generation-pool)
          (let [^NotificationEmitter notification-emitter (ManagementFactory/getMemoryMXBean)
                usage-threshold (-> tenured-generation-pool .getUsage .getMax (* memory-usage-threshold) long)]
            (log/info :message "Adding post-gc low-memory notification listener to tenured generation pool."
                      :threshold-megabytes (quot usage-threshold (* 1024 1024)))
            (.setCollectionUsageThreshold tenured-generation-pool usage-threshold)
            (.addNotificationListener notification-emitter notification-listener nil nil)))))))

(defn add-callback!
  "Add a callback function that will be invoked when low memory conditions are
  detected. The callback-fn should take no arguments, and should do whatever it
  can to clean up non-critical resources that might be hogging memory."
  [callback-fn]
  (assert (ifn? callback-fn))
  (ensure-notification-listener!)
  (swap! callback-fns-atom conj callback-fn)
  nil)

(defn remove-callback!
  "Remove a callback function from the list of functions that will be invoked
  when low memory conditions are detected."
  [callback-fn]
  (assert (ifn? callback-fn))
  (swap! callback-fns-atom
         (fn [callback-fns]
           (filterv (partial not= callback-fn)
                    callback-fns)))
  nil)

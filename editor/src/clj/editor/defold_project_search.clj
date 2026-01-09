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

(ns editor.defold-project-search
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]]
            [util.text-util :as text-util]
            [util.thread-util :as thread-util])
  (:import [java.util ArrayList]
           [java.util.concurrent LinkedBlockingQueue]))

(set! *warn-on-reflection* true)

(defn- search-data-sort-key [entry]
  (some-> entry :resource resource/proj-path))

(defn make-search-data-future [report-error! project]
  (let [evaluation-context (g/make-evaluation-context)]
    (future
      (try
        (let [search-data
              (->> (g/node-value project :node-id+resources evaluation-context)
                   (into []
                         (keep (fn [[node-id resource]]
                                 (when (and (resource/loaded? resource)
                                            (not (resource/internal? resource))
                                            (= :file (resource/source-type resource)))
                                   (let [resource-type (resource/resource-type resource)
                                         search-value-fn (when (:search-fn resource-type)
                                                           (:search-value-fn resource-type))]
                                     (when (and search-value-fn
                                                (resource/exists? resource)
                                                (resource/textual? resource))
                                       (let [search-value (search-value-fn node-id resource evaluation-context)]
                                         (when-not (g/error? search-value)
                                           {:resource resource
                                            :search-value search-value}))))))))
                   (sort-by search-data-sort-key))]
          (ui/run-later
            (project/update-system-cache-save-data! evaluation-context)
            (project/log-cache-info! (g/cache) "Cached searched save values in system cache."))
          search-data)
        (catch Throwable error
          (report-error! error)
          nil)))))

(defn- resource-matches-library-setting? [resource include-libraries?]
  (or include-libraries?
      (resource/file-resource? resource)))

(defn- resource-matches-file-ext? [resource file-ext-pats]
  (or (empty? file-ext-pats)
      (let [ext (resource/type-ext resource)]
        (some #(.find (re-matcher % ext))
              file-ext-pats))))

(defn- make-resource-type->matches-fn [workspace search-string]
  (let [search-fn->matches-fn
        (into {}
              (comp (mapcat #(vals (workspace/get-resource-type-map workspace %)))
                    (keep :search-fn)
                    (distinct)
                    (map (fn [search-fn]
                           (let [pattern (search-fn search-string)
                                 matches-fn #(search-fn % pattern)]
                             (pair search-fn matches-fn)))))
              [:editable :non-editable])]

    (fn resource-type->matches-fn [resource-type]
      (some-> resource-type :search-fn search-fn->matches-fn))))

(defn- make-search-resource? [searched-exts search-libraries]
  {:pre [(boolean? search-libraries)]}
  (let [file-ext-patterns
        (into []
              (comp (remove empty?)
                    (distinct)
                    (map #(text-util/search-string->re-pattern (string/replace % #"\*?\." "") :case-insensitive)))
              (some-> searched-exts
                      (string/replace #" " "")
                      (string/split #",")))]
    (fn search-resource? [resource]
      (and (resource/loaded? resource)
           (resource-matches-library-setting? resource search-libraries)
           (resource-matches-file-ext? resource file-ext-patterns)))))

(defn- start-search-thread [report-error! search-data-future resource-type->matches-fn search-resource? produce-fn]
  {:pre [(ifn? search-resource?)]}
  (future
    (try
      (let [xform (keep (fn [search-data]
                          (thread-util/throw-if-interrupted!)
                          (let [resource (:resource search-data)]
                            (when (search-resource? resource)
                              (let [resource-type (resource/resource-type resource)
                                    matches-fn (resource-type->matches-fn resource-type)
                                    matches (when matches-fn
                                              (matches-fn (:search-value search-data)))]
                                (when-not (coll/empty? matches)
                                  {:resource resource
                                   :matches matches}))))))]
        (run! produce-fn (sequence xform (deref search-data-future)))
        (produce-fn ::done))
      (catch InterruptedException _
        ;; future-cancel was invoked from another thread.
        nil)
      (catch Throwable error
        (report-error! error)
        nil))))

(defn make-file-searcher
  "Returns a map of two functions, start-search! and abort-search! that can be
  used to perform asynchronous search queries against an ordered sequence of
  search data. The search data is expected to be sorted in the order
  it should appear in the search results list, and entries are expected to have
  :resource (Resource) and :search-value (anything) fields.
  When start-search! is called, it will cancel any pending search and start a
  new search using the provided search-string and searched-exts arguments. It
  will call stop-consumer! with the value returned from the last call to
  start-consumer!, then start-consumer! will be called with a poll function as
  its sole argument. The value returned by start-consumer! will be stored and
  used as the argument to stop-consumer! when a search is aborted. The consumer
  is expected to periodically call the supplied poll function to consume search
  results. It will either return nil if there is no result currently available,
  the namespaced keyword :defold-project-search/done if the search has completed
  and there will be no more results, or a single match consisting of
  [Resource, [match-info, ...]], where match-info is a map of view-specific data
  that can be used to highlight the specific match. For example, the match-info
  for code editor resources contain the matched :row, :start-col and :end-col.
  When abort-search! is called, any spawned background threads will terminate,
  and if there was a previous consumer, stop-consumer! will be called with it.
  Since many operations happen on a background thread, report-error! will be
  called with the Throwable in the event of an error."
  [workspace search-data-future start-consumer! stop-consumer! report-error!]
  (let [pending-search-atom (atom nil)
        abort-search! (fn [pending-search]
                        (some-> pending-search :thread future-cancel)
                        (some-> pending-search :consumer stop-consumer!)
                        nil)
        start-search! (fn [pending-search search-string searched-exts search-libraries]
                        (abort-search! pending-search)
                        (if (seq search-string)
                          (let [queue (LinkedBlockingQueue. 1024)
                                produce-fn #(do
                                              (.put queue %))
                                consume-fn #(let [results (ArrayList.)]
                                              (.drainTo queue results)
                                              (seq results))
                                resource-type->matches-fn (make-resource-type->matches-fn workspace search-string)
                                search-resource? (make-search-resource? searched-exts search-libraries)
                                thread (start-search-thread report-error! search-data-future resource-type->matches-fn search-resource? produce-fn)
                                consumer (start-consumer! consume-fn)]
                            {:thread thread
                             :consumer consumer})
                          (do (start-consumer! (constantly [::done]))
                              nil)))]
    {:start-search! (fn [search-string searched-exts include-libraries?]
                      (try
                        (swap! pending-search-atom start-search! search-string searched-exts include-libraries?)
                        (catch Throwable error
                          (report-error! error)))
                      nil)
     :abort-search! (fn []
                      (try
                        (swap! pending-search-atom abort-search!)
                        (catch Throwable error
                          (report-error! error)))
                      nil)}))

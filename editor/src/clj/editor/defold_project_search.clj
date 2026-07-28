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
            [util.fn :as fn]
            [util.text-util :as text-util]
            [util.thread-util :as thread-util])
  (:import [java.util ArrayList]
           [java.util.concurrent CancellationException LinkedBlockingQueue]))

(set! *warn-on-reflection* true)

(defn- search-data-sort-key [entry]
  (some-> entry :resource resource/proj-path))

(defn make-search-data-future
  "Returns a future yielding search data for every project resource that passes
  search-resource?, each with its :search-value built and ready to match against."
  [report-error! project search-resource?]
  (let [evaluation-context (g/make-evaluation-context)]
    (future
      (try
        (let [search-data
              (->> (g/node-value project :node-id+resources evaluation-context)
                   (into []
                         (keep (fn [[node-id resource]]
                                 (thread-util/throw-if-interrupted!)
                                 (when (and (resource/loaded? resource)
                                            (not (resource/internal? resource))
                                            (= :file (resource/source-type resource))
                                            (search-resource? resource))
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
        (catch InterruptedException _
          ;; future-cancel was invoked from another thread.
          (ui/run-later
            (project/update-system-cache-save-data! evaluation-context))
          nil)
        (catch Throwable error
          (ui/run-later
            (project/update-system-cache-save-data! evaluation-context))
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

(defn- parse-searched-exts [searched-exts]
  (let [searched-exts (some-> searched-exts
                              (string/replace #" " "")
                              (string/split #","))]
    (coll/into-> searched-exts []
      (remove empty?)
      (map #(string/replace % #"\*?\." ""))
      (distinct)
      (map #(text-util/search-string->re-pattern % :case-insensitive)))))

(defn- make-search-resource? [file-ext-patterns search-libraries]
  {:pre [(boolean? search-libraries)]}
  (fn search-resource? [resource]
    (and (resource-matches-library-setting? resource search-libraries)
         (resource-matches-file-ext? resource file-ext-patterns))))

(defn- start-search-thread [report-error! search-data-future resource-type->matches-fn produce-fn]
  (future
    (try
      (let [xform (keep (fn [entry]
                          (thread-util/throw-if-interrupted!)
                          (let [resource (:resource entry)
                                resource-type (resource/resource-type resource)
                                matches-fn (resource-type->matches-fn resource-type)
                                matches (when matches-fn
                                          (matches-fn (:search-value entry)))]
                            (when-not (coll/empty? matches)
                              {:resource resource
                               :matches matches}))))]
        (run! produce-fn (sequence xform (deref search-data-future)))
        (produce-fn ::done))
      (catch InterruptedException _
        ;; future-cancel was invoked from another thread.
        nil)
      (catch CancellationException _
        nil)
      (catch Throwable error
        (report-error! error)
        nil))))

(defn make-file-searcher
  "Returns a map of two functions, start-search! and abort-search! that can be
  used to perform asynchronous search queries. Search content is prepared per
  ext/library filter and rebuilt only when that filter changes; search-pattern
  keystrokes match against the pre-built content.
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
  [workspace project start-consumer! stop-consumer! report-error!]
  (let [pending-search-atom (atom nil)

        active-search-data-atom (atom nil)
        prepare-search-data!
        (fn/memoize
          {:limit 1}
          (fn [searched-exts search-libraries]
            (make-search-data-future
              report-error! project
              (make-search-resource?
                (parse-searched-exts searched-exts)
                search-libraries))))

        abort-search! (fn [pending-search]
                        (some-> pending-search :thread future-cancel)
                        (some-> pending-search :consumer stop-consumer!)
                        nil)
        start-search! (fn [pending-search search-data-future search-string]
                        (abort-search! pending-search)
                        (if (coll/not-empty search-string)
                          (let [queue (LinkedBlockingQueue. 1024)
                                produce-fn #(do
                                              (.put queue %))
                                consume-fn #(let [results (ArrayList.)]
                                              (.drainTo queue results)
                                              (seq results))
                                resource-type->matches-fn (make-resource-type->matches-fn workspace search-string)
                                thread (start-search-thread report-error! search-data-future resource-type->matches-fn produce-fn)
                                consumer (start-consumer! consume-fn)]
                            {:thread thread
                             :consumer consumer})
                          (do (start-consumer! (constantly [::done]))
                              nil)))]
    {:start-search! (fn [search-string searched-exts include-libraries?]
                      (try
                        (let [search-data-future (prepare-search-data! searched-exts include-libraries?)
                              [previous-search-data-future] (swap-vals! active-search-data-atom (constantly search-data-future))]
                          (when (and previous-search-data-future
                                     (not (identical? previous-search-data-future search-data-future)))
                            (future-cancel previous-search-data-future))
                          (swap! pending-search-atom start-search! search-data-future search-string))
                        (catch Throwable error
                          (report-error! error)))
                      nil)
     :abort-search! (fn []
                      (try
                        (swap! pending-search-atom abort-search!)
                        (fn/clear-memoized! prepare-search-data!)
                        (let [[search-data-future] (swap-vals! active-search-data-atom (constantly nil))]
                          (some-> search-data-future future-cancel))
                        (catch Throwable error
                          (report-error! error)))
                      nil)}))

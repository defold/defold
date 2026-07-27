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

(defn make-search-data-future
  "Returns a map of :data-future and :cancel!. Deref :data-future for search data
  covering every project resource that passes search-resource?, each with its
  :search-value built and ready to match against. Call :cancel! to abandon the
  work without reporting an error."
  [report-error! project search-resource?]
  (let [evaluation-context (g/make-evaluation-context)

        ;; Interrupting the thread can surface as any exception, not just
        ;; InterruptedException, so we record that we cancelled rather than
        ;; guessing from the exception type.
        cancelled (volatile! false)

        data-future
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
            (catch Throwable error
              ;; Keep the save values we managed to build, so the next filter
              ;; does not have to recompute them from scratch.
              (ui/run-later
                (project/update-system-cache-save-data! evaluation-context))
              (when-not @cancelled
                (report-error! error))
              nil)
            (finally
              (Thread/interrupted))))]
    {:data-future data-future
     :cancel! (fn cancel! []
                (vreset! cancelled true)
                (future-cancel data-future))}))

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
                 (distinct))))

(defn- make-search-resource? [searched-ext-strings search-libraries]
  {:pre [(boolean? search-libraries)]}
  (let [file-ext-patterns
        (coll/into-> searched-ext-strings []
                     (map #(text-util/search-string->re-pattern % :case-insensitive)))]
    (fn search-resource? [resource]
      (and (resource/loaded? resource)
           (resource-matches-library-setting? resource search-libraries)
           (resource-matches-file-ext? resource file-ext-patterns)))))

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
        nil)
      (catch java.util.concurrent.CancellationException _
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
  [workspace project initial-searched-exts initial-search-libraries start-consumer! stop-consumer! report-error!]
  (let [pending-search-atom (atom nil)

        ;; Cached prep future, keyed by the ext/library filter. When the filter
        ;; changes, the now-obsolete future is cancelled so its whole-project
        ;; work does not keep running in the background. The future is created
        ;; outside any swap so a retry cannot leave an orphan running.
        search-data-atom (atom nil)
        prepare-search-data!
        (fn [searched-exts search-libraries]
          (let [searched-ext-strings (parse-searched-exts searched-exts)
                filter-key [searched-ext-strings search-libraries]
                current @search-data-atom]
            (if (= (:key current) filter-key)
              (:data-future current)
              (let [{:keys [data-future cancel!]}
                    (make-search-data-future
                      report-error! project
                      (make-search-resource? searched-ext-strings search-libraries))]
                (reset! search-data-atom {:key filter-key :data-future data-future :cancel! cancel!})
                (when current
                  ((:cancel! current)))
                data-future))))

        abort-search! (fn [pending-search]
                        (some-> pending-search :thread future-cancel)
                        (some-> pending-search :consumer stop-consumer!)
                        nil)
        start-search! (fn [pending-search search-data-future search-string]
                        (abort-search! pending-search)
                        (if search-data-future
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
    (prepare-search-data! initial-searched-exts initial-search-libraries)
    {:start-search! (fn [search-string searched-exts include-libraries?]
                      (try
                        (let [search-data-future (prepare-search-data! searched-exts include-libraries?)]
                          (swap! pending-search-atom start-search!
                                 (when (coll/not-empty search-string) search-data-future)
                                 search-string))
                        (catch Throwable error
                          (report-error! error)))
                      nil)
     :abort-search! (fn []
                      (try
                        (swap! pending-search-atom abort-search!)
                        (when-let [current @search-data-atom]
                          (reset! search-data-atom nil)
                          ((:cancel! current)))
                        (catch Throwable error
                          (report-error! error)))
                      nil)}))

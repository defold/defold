(ns editor.defold-project-search
  (:require
   [clojure.java.io :as io]
   [clojure.string :as string]
   [dynamo.graph :as g]
   [editor.resource :as resource]
   [editor.ui :as ui]
   [util.text-util :as text-util]
   [util.thread-util :as thread-util])
  (:import
   (clojure.lang IReduceInit)
   (java.io BufferedReader StringReader)
   (java.util.concurrent LinkedBlockingQueue)
   (java.util.regex Pattern)))

(set! *warn-on-reflection* true)

(defn- make-line-coll
  [make-buffered-reader]
  (reify IReduceInit
    (reduce [_ rf init]
      (with-open [^BufferedReader rdr (make-buffered-reader)]
        (loop [ret init
               row 0
               pos 0]
          (if-some [line (.readLine rdr)]
            (let [ret (rf ret {:line line
                               :row row
                               :pos pos})]
              (if (reduced? ret)
                @ret
                (recur ret (inc row) (+ pos (count line)))))
            ret))))))

(defn- line-coll
  [{:keys [content resource] :as save-data}]
  (or (when (some? content)
        (make-line-coll #(BufferedReader. (StringReader. content))))
      (when (and (resource/exists? resource) (not (text-util/binary? resource)))
        (make-line-coll #(io/reader resource)))))

(defn compile-find-in-files-regex
  "Convert a search-string to a case-insensitive java regex."
  [search-str]
  (let [clean-str (->> (string/split search-str #"\*")
                       (map #(Pattern/quote %))
                       (string/join ".*"))]
    (re-pattern (str "(?i)^(.*)(" clean-str ")(.*)$"))))

(defn- find-matches [pattern save-data]
  (when-some [lines (line-coll save-data)]
    (into []
          (comp (keep (fn [{:keys [line row pos]}]
                        (let [matcher (re-matcher pattern line)]
                          (when (.matches matcher)
                            {:line row
                             :caret-position pos
                             :match (str (apply str (take-last 24 (string/triml (.group matcher 1))))
                                         (.group matcher 2)
                                         (apply str (take 24 (string/trimr (.group matcher 3)))))}))))
                (take 10))
          lines)))

(defn- save-data-sort-key [entry]
  (some-> entry :resource resource/proj-path))

(defn make-file-resource-save-data-future [report-error! project]
  (let [evaluation-context (g/make-evaluation-context)]
    (future
      (try
        (let [save-data (->> (into []
                                     (keep (fn [node-id]
                                             (let [save-data (g/node-value node-id :save-data evaluation-context)
                                                   resource (:resource save-data)]
                                               (when (and (some? resource) (= :file (resource/source-type resource)))
                                                 save-data))))
                                     (g/node-value project :nodes evaluation-context))
                               (sort-by save-data-sort-key))]
          (ui/run-later (g/update-cache-from-evaluation-context! evaluation-context))
          save-data)
        (catch Throwable error
          (report-error! error)
          nil)))))

(defn- start-search-thread [report-error! file-resource-save-data-future term exts produce-fn]
  (future
    (try
      (let [pattern (compile-find-in-files-regex term)
            file-ext-pats (into []
                                (comp (remove empty?)
                                      (distinct)
                                      (map #(compile-find-in-files-regex (string/replace % #"\*?\." ""))))
                                (some-> exts
                                        (string/replace #" " "")
                                        (string/split #",")))
            xform (comp (map thread-util/abortable-identity!)
                        (filter (fn [{:keys [resource]}]
                                  (or (empty? file-ext-pats)
                                      (let [ext (resource/ext resource)]
                                        (some #(re-matches % ext) file-ext-pats)))))
                        (map (fn [{:keys [resource] :as save-data}]
                               {:resource resource
                                :matches (find-matches pattern save-data)}))
                        (filter #(seq (:matches %))))]
        (run! produce-fn (sequence xform (deref file-resource-save-data-future)))
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
  file resource save data. The save data is expected to be sorted in the order
  it should appear in the search results list, and entires are expected to have
  :resource (Resource) and :content (String) fields.
  When start-search! is called, it will cancel any pending search and start a
  new search using the provided term and exts String arguments. It will call
  stop-consumer! with the value returned from the last call to start-consumer!,
  then start-consumer! will be called with a poll function as its sole argument.
  The value returned by start-consumer! will be stored and used as the argument
  to stop-consumer! when a search is aborted. The consumer is expected to
  periodically call the supplied poll function to consume search results.
  It will either return nil if there is no result currently available, the
  namespaced keyword :defold-project-search/done if the search has completed
  and there will be no more results, or a single match consisting of
  [Resource, [{:line long, :caret-position long, :match String}, ...]].
  When abort-search! is called, any spawned background threads will terminate,
  and if there was a previous consumer, stop-consumer! will be called with it.
  Since many operations happen on a background thread, report-error! will be
  called with the Throwable in the event of an error."
  [file-resource-save-data-future start-consumer! stop-consumer! report-error!]
  (let [pending-search-atom (atom nil)
        abort-search! (fn [pending-search]
                        (some-> pending-search :thread future-cancel)
                        (some-> pending-search :consumer stop-consumer!)
                        nil)
        start-search! (fn [pending-search term exts]
                        (abort-search! pending-search)
                        (if (seq term)
                          (let [queue (LinkedBlockingQueue. 1024)
                                produce-fn #(do
                                              (.put queue %))
                                consume-fn #(let [results (java.util.ArrayList.)]
                                              (.drainTo queue results)
                                              (seq results))
                                thread (start-search-thread report-error! file-resource-save-data-future term exts produce-fn)
                                consumer (start-consumer! consume-fn)]
                            {:thread thread
                             :consumer consumer})
                          (do (start-consumer! (constantly [::done]))
                              nil)))]
    {:start-search! (fn [term exts]
                      (try
                        (swap! pending-search-atom start-search! term exts)
                        (catch Throwable error
                          (report-error! error)))
                      nil)
     :abort-search! (fn []
                      (try
                        (swap! pending-search-atom abort-search!)
                        (catch Throwable error
                          (report-error! error)))
                      nil)}))

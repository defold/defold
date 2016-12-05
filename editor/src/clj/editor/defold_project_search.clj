(ns editor.defold-project-search
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.resource :as resource]
            [util.thread-util :as thread-util])
  (:import (java.util.concurrent LinkedBlockingQueue)))

(defn compile-find-in-files-regex
  "Convert a search-string to a java regex"
  [search-str]
  (let [clean-str (string/replace search-str #"[\<\(\[\{\\\^\-\=\$\!\|\]\}\)\?\+\.\>]" "")]
    (re-pattern (format "^(.*)(%s)(.*)$"
                        (string/lower-case
                          (string/replace clean-str #"\*" ".*"))))))

(defn- line->caret-pos [content line]
  (let [line-counts (map (comp inc count) (string/split-lines content))]
    (reduce + (take line line-counts))))

(defn- make-file-resource-save-data-future [report-error! project]
  (let [basis (g/now)
        cache (atom (deref (g/cache)))
        options {:basis basis :cache cache :skip-validation true}]
    (future
      (try
        (filterv #(some-> % :resource resource/source-type (= :file))
                 (g/node-value project :save-data options))
        (catch Throwable error
          (report-error! error))))))

(defn- save-data-sort-key [entry]
  (some-> entry :resource resource/proj-path))

(defn- filter-save-data-future [report-error! unfiltered-save-data-future exts]
  (println exts "filter save-data thread started")
  (try
    (let [file-ext-pats (into []
                              (comp (remove empty?)
                                    (distinct)
                                    (map compile-find-in-files-regex))
                              (some-> exts
                                      (string/replace #" " "")
                                      (string/split #",")))
          unfiltered-save-data (deref unfiltered-save-data-future)
          filtered-save-data (if (empty? file-ext-pats)
                               unfiltered-save-data
                               (into []
                                     (comp (map thread-util/abortable-identity!)
                                           (filter (fn [entry]
                                                     (let [rext (-> entry :resource resource/ext)]
                                                       (some #(re-matches % rext) file-ext-pats)))))
                                     unfiltered-save-data))
          sorted-filtered-save-data (sort-by save-data-sort-key filtered-save-data)]
      (println exts "filter save-data thread completed")
      sorted-filtered-save-data)
    (catch InterruptedException _
      ; future-cancel was invoked from another thread.
      (println exts "filter save-data thread interrupted")
      nil)
    (catch Throwable error
      (report-error! error)
      nil)))

(defn- start-search-thread [report-error! filtered-save-data-future term produce-fn]
  (future
    (println term "search thread started")
    (try
      (let [pattern (compile-find-in-files-regex term)
            xform (comp (map thread-util/abortable-identity!)
                        (filter :content)
                        (map (fn [{:keys [content] :as hit}]
                               (assoc hit :matches
                                          (->> (string/split-lines (:content hit))
                                               (map string/lower-case)
                                               (keep-indexed (fn [idx l]
                                                               (let [[_ pre m post] (re-matches pattern l)]
                                                                 (when m
                                                                   {:line           idx
                                                                    :caret-position (line->caret-pos (:content hit) idx)
                                                                    :match          (str (apply str (take-last 24 (string/triml pre)))
                                                                                         m
                                                                                         (apply str (take 24 (string/trimr post))))}))))
                                               (take 10)))))
                        (filter #(seq (:matches %))))]
        (doseq [entry (sequence xform (deref filtered-save-data-future))]
          (produce-fn entry))
        (println term "search thread completed"))
      (catch InterruptedException _
        ; future-cancel was invoked from another thread.
        (println term "search thread interrupted")
        )
      (catch Throwable error
        (report-error! error)
        nil))))

(defn make-file-searcher [project start-consumer! stop-consumer! report-error!]
  (let [unfiltered-save-data-future (make-file-resource-save-data-future report-error! project)
        [start-filter! abort-filter!] (let [filter-fn (partial filter-save-data-future report-error! unfiltered-save-data-future)
                                            start-filter! (thread-util/make-cached-future-fn filter-fn)
                                            abort-filter! #(thread-util/cancel-cached-future-fn start-filter!)]
                                        [start-filter! abort-filter!])
        pending-search-atom (atom nil)
        abort-search! (fn [pending-search]
                        (some-> pending-search :thread future-cancel)
                        (some-> pending-search :consumer stop-consumer!)
                        nil)
        start-search! (fn [pending-search term exts]
                        (abort-search! pending-search)
                        (let [filtered-save-data-future (start-filter! exts)]
                          (if (seq term)
                            (let [queue (LinkedBlockingQueue. 10)
                                  thread (start-search-thread report-error! filtered-save-data-future term #(.put queue %))
                                  consumer (start-consumer! #(.poll queue))]
                              {:thread thread
                               :consumer consumer})
                            (let [consumer (start-consumer! (constantly nil))]
                                (stop-consumer! consumer)
                                nil))))]
    {:start-search! (partial swap! pending-search-atom start-search!)
     :abort-search! (fn []
                      (swap! pending-search-atom abort-search!)
                      (abort-filter!))}))

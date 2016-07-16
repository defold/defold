(ns editor.hot-reload
  (:require [editor
             [defold-project :as project]
             [resource :as resource]]
            [util.digest :as digest]))

(set! *warn-on-reflection* true)

(defn- handler [project {:keys [url method headers]}]
  (let [node-name (subs url (count project/hot-reload-url-prefix))
        node      (project/get-resource-node project "/game.project")
        res       (project/build project node {})
        resources (into {} (keep (fn [d]
                                   (when-let [r (:resource d)]
                                     [(resource/proj-path r) (:content d)]))
                                 res))]
    (if-let [c (get resources node-name)]
      (let [etag (digest/sha1->hex c)
            remote-etag (first (get headers "If-none-match"))
            cached? (= etag remote-etag)
            response-headers (cond-> {"Content-Type" "application/octet-stream"
                                      "ETag" etag}
                               (= method "GET") (assoc "Content-Length" (str (count c))))]
        (cond-> {:code (if cached? 304 200)
                 :headers response-headers}
          (and (= method "GET") (not cached?)) (assoc :body c)))
      {:code 404})))

(defn build-handler [project req-headers]
  (handler project req-headers))

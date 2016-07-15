(ns editor.hot-reload
  (:require [editor
             [defold-project :as project]
             [resource :as resource]]))

(set! *warn-on-reflection* true)

(defn build-handler [project]
  (fn [{:keys [url]}]
    (let [node-name (subs url (count project/hot-reload-url-prefix))
          node      (project/get-resource-node project "/game.project")
          res       (project/build project node {})
          resources (into {} (keep (fn [d]
                                     (when-let [r (:resource d)]
                                       [(resource/proj-path r) (:content d)]))
                                   res))]
    (if-let [c (get resources node-name)]
      {:code    200
       :headers {"Content-Type" "application/octet-stream"}
       :body    c}
      {:code 404}))))

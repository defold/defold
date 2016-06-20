(ns editor.hot-reload
  (:require [editor.defold-project :as project]
            [editor.resource :as resource]))

(def ^:const url-prefix "/build")

(defn build-handler [project]
  (fn [{:keys [url] :as req}]
    (let [node-name (subs url (count url-prefix))
          node      (project/get-resource-node project node-name)
          res       (project/build project node {})
          resources (into {} (keep (fn [d]
                                     (when-let [r (:resource (:resource d))]
                                       [(resource/proj-path r) (:content d)]))
                                   res))]
      (if-let [c (get resources node-name)]
        {:code    200
         :headers {"Content-Type" "application/octet-stream"}
         :body    c}
        {:code 404}))))

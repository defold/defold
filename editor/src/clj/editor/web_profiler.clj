(ns editor.web-profiler
  (:require [editor.handler :as handler]
            [util.http-server :as http-server])
  (:import  [com.defold.util Profiler]
            [java.io File]
            [java.awt Desktop]
            [java.net URI]))

(def ^:private template-path "resources/profiler_template.html")
(def ^:private target-path "tmp/profiler.html")

(defn- dump-profiler []
  (let [data (Profiler/dumpJson)
        html (-> (slurp template-path)
               (clojure.string/replace "$PROFILER_DATA" data))]
    (-> (File. ^String target-path)
      .getParentFile
      .mkdirs)
    (spit target-path html)))

(handler/defhandler :profile :global
  (enabled? [] true)
  (run [] (dump-profiler)))

(handler/defhandler :profile-show :global
  (enabled? [] true)
  (run [web-server]
       (dump-profiler)
       (.browse (Desktop/getDesktop) (URI. (format "%s/profiler" (http-server/local-url web-server))))))

(defn handler [req]
  {:code 200
   :headers {"Content-Type" "text/html"}
   :body (try
           (slurp target-path)
           (catch Throwable e
             (dump-profiler)
             (slurp target-path)))})

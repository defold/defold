(ns editor.web-profiler
  (:require [editor.handler :as handler]
            [editor.ui :as ui]
            [util.http-server :as http-server]
            [clojure.java.io :as io])
  (:import  [com.defold.util Profiler]))

(def ^:private template-path "profiler_template.html")
(def ^:private html (atom nil))

(defn- dump-profiler []
  (reset! html (-> (slurp (io/resource template-path))
                   (clojure.string/replace "$PROFILER_DATA" (Profiler/dumpJson)))))

(handler/defhandler :profile :global
  (enabled? [] true)
  (run [] (dump-profiler)))

(handler/defhandler :profile-show :global
  (enabled? [] true)
  (run [web-server]
       (dump-profiler)
       (ui/open-url (format "%s/profiler" (http-server/local-url web-server)))))

(defn handler [req]
  {:code 200
   :headers {"Content-Type" "text/html"}
   :body (or @html (dump-profiler))})

(ns docs
  (:use [clojure-ini.core :only [read-ini]])
  (:require [clojure.test :refer :all]
            [codox.main]
            [aws.sdk.s3 :as s3])
  (:import [java.io File]))

(def public-namespaces ['dynamo.buffers
                        'dynamo.camera
                        'dynamo.editors
                        'dynamo.file
                        'dynamo.geom
                        'dynamo.gl
                        'dynamo.image
                        'dynamo.node
                        'dynamo.outline
                        'dynamo.particle-lib
                        'dynamo.project
                        'dynamo.property
                        'dynamo.selection
                        'dynamo.system
                        'dynamo.texture
                        'dynamo.types
                        'dynamo.ui
                        'dynamo.util])

; NOTE: The source-folder is absolute and root isn't set due to a bug
; in codex (root isn't used when locating files). For the time being and until codox
; is fixed we've to have it this way.
(def codox-options
  {:language   :clojure
   :sources    [(-> (File. (System/getProperty "user.dir"))
                  (.toPath)
                  (.getParent)
                  (.normalize)
                  (.resolve "com.dynamo.cr.sceneed2/src")
                  (.toString))]
   :output-dir "doc"
   :include public-namespaces
   :src-dir-uri "https://github.com/relevance/defold/blob/clojure-sceneed/com.dynamo.cr/com.dynamo.cr.sceneed2/"
   :src-linenum-anchor-prefix "L"
   :defaults {:doc/format :markdown}})

(defn- upload-docs []
  (let [s3cfg (format "%s/.s3cfg" (System/getProperty "user.home"))
        cfg (read-ini s3cfg :keywordize? true)
        cred {:access-key (get-in cfg [:default :access_key])
              :secret-key (get-in cfg [:default :secret_key])}
        files (->> (file-seq (File. "doc"))
                (filter #(.isFile ^File %)))]
    (doseq [^File f files]
      (println "uploading" (.getPath f))
      (s3/put-object cred "defold-clojuredocs" (.getPath f) f))))

(deftest doc-test
  (testing "doc-test"
           (codox.main/generate-docs codox-options)
           (when (= "builder" (.get (System/getenv) "USER"))
             (upload-docs))))

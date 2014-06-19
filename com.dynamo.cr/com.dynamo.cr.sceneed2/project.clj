(defproject com.dynamo.cr.sceneed2 "0.1.0"
  :description "Dependency list for eleiko project"
  :url ""
  :license {:name "All Rights Reserved" }
  :dependencies [[org.clojure/clojure "1.6.0"]
                 [org.clojure/core.typed "0.2.42"]
                 [prismatic/schema "0.2.3"]
                 [prismatic/plumbing "0.3.1"]
                 [org.clojure/tools.nrepl "0.2.3"]
                 [org.clojure/test.check "0.5.8"]]
  :plugins [[codox "0.8.9"]
            [lein-ubersource "0.1.1"]
            [lein-simpleton "1.3.0"]]
  :codox {:include [dynamo.node dynamo.ui]
          :src-dir-uri "https://github.com/relevance/defold/blob/clojure-sceneed/com.dynamo.cr/com.dynamo.cr.sceneed2/"
          :src-linenum-anchor-prefix "L"}
  :resource-paths []
  :source-paths ^:top-displace []
  :target-path "lib"
  :test-paths []
  :clean-targets [:target-path "doc"]
  :profiles {:deps {:source-paths []}
             :docs {:source-paths ["src/clj"]}}
  :aliases  {"update-deps" ["do" "deps," "uberjar," "ubersource"]
             "serve-docs"  ["with-profile" "docs" "do" "doc," "simpleton" "5000" "file" ":from" "doc"]}
  )

(def classpath-file ".eclipse-classpath")
(def classpath-elements (clojure.string/split (slurp (clojure.java.io/resource classpath-file)) #":"))
(def filters [ #".*/src/clj$" #".*/src/java$" #".*/test/clj$" #".*/bin$"])
(def project (assoc project :resource-paths (remove (fn [path] (some #(re-matches % path) filters)) classpath-elements)))

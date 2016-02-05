(defproject defold-editor "2.0.0-SNAPSHOT"
  :description      "Defold game editor"
  :url              "http://doc.defold.com"

  :repositories     {"local" ~(str (.toURI (java.io.File. "localjars")))}

  :plugins          [[lein-protobuf-minimal "0.4.4" :hooks false]
                     [codox "0.9.3"]]

  :pedantic?        :abort
  :dependencies     [[org.clojure/clojure                         "1.8.0"]
                     [org.clojure/core.cache                      "0.6.4"]
                     [org.clojure/core.match                      "0.2.2"]
                     [org.clojure/tools.macro                     "0.1.5"]
                     [org.clojure/tools.namespace                 "0.2.10"]
                     [org.clojure/data.json                       "0.2.6"]
                     [com.cognitect/transit-clj                   "0.8.285"]
                     [prismatic/schema                            "1.0.4"]
                     [prismatic/plumbing                          "0.5.2"]
                     [inflections/inflections                     "0.10.0"]
                     [com.google.protobuf/protobuf-java           "2.3.0"]
                     [org.slf4j/slf4j-api                         "1.7.14"]
                     [ch.qos.logback/logback-classic              "1.1.3"]
                     [joda-time/joda-time                         "2.9.2"]
                     [commons-io/commons-io                       "2.4"]
                     [org.clojure/data.json                       "0.2.6"]
                     [org.projectodd.shimdandy/shimdandy-api      "1.2.0"]
                     [org.projectodd.shimdandy/shimdandy-impl     "1.2.0"]
                     [potemkin                                    "0.4.3"]
                     [com.nanohttpd/nanohttpd                     "2.1.1"]
                     [com.sun.jersey/jersey-core                  "1.19"]
                     [com.sun.jersey/jersey-client                "1.19"]
                     [javax.vecmath/vecmath                       "1.5.2"]

                     ;; Keep jna version in sync with bundle.py. See JNA_VERSION
                     [net.java.dev.jna/jna                        "4.1.0"]
                     [net.java.dev.jna/jna-platform               "4.1.0"]

                     ;; Keep jogl version in sync with bundle.py. See JOGL_VERSION
                     [org.jogamp.gluegen/gluegen-rt-main          "2.0.2"]
                     [org.jogamp.jogl/jogl-all-main               "2.0.2"]

                     ;; NOTE: Ancient libraries, not available via Maven
                     [org.codehaus.jackson/jackson-core-asl       "1.9.13"]
                     [org.codehaus.jackson/jackson-mapper-asl     "1.9.13"]
                     [org.eclipse.jgit/org.eclipse.jgit           "4.0.0.201505050340-m2"]

                     ;; Dev REPL suff
                     [org.clojure/tools.nrepl                     "0.2.12"]
                     [cider/cider-nrepl                           "0.10.2" :exclusions [org.clojure/tools.nrepl]]
                     [refactor-nrepl                              "1.2.0" :exclusions [org.clojure/tools.nrepl]]
                     ]

  :source-paths      ["src/clj"
                      "../com.dynamo.cr/com.dynamo.cr.sceneed2/src/clj"]

  :java-source-paths ["src/java"
                      "../com.dynamo.cr/com.dynamo.cr.sceneed2/src/java"]

  :resource-paths    ["resources" "generated-resources"]

  :proto-paths       ["../com.dynamo.cr/com.dynamo.cr.common/proto"
                      "../com.dynamo.cr/com.dynamo.cr.rlog/proto"
                      "../engine/ddf/src"
                      "../engine/engine/proto"
                      "../engine/gameobject/proto"
                      "../engine/gamesys/proto"
                      "../engine/graphics/proto"
                      "../engine/input/proto"
                      "../engine/particle/proto/particle"
                      "../engine/render/proto/render"
                      "../engine/resource/proto"
                      "../engine/script/src"
                      "../engine/vscript/proto"]

  :protobuf-includes ["../engine/gamesys/proto"
                      "../engine/ddf/src"
                      "../engine/script/src"
                      ~(str (System/getenv "DYNAMO_HOME") "/ext/include")]

  :protobuf-exclude  ["../engine/ddf/src/test"]

  :aliases           {"ci"        ["do" "test," "uberjar"]
                      "benchmark" ["with-profile" "+test" "trampoline" "run" "-m" "benchmark.graph-benchmark"]}

  :codox             {:sources                   ["src/clj"]
                      :output-dir                "target/doc/api"
                      :exclude                   [internal]
                      :src-dir-uri               "http://github.com/relevance/defold/blob/clojure-sceneed"
                      :src-linenum-anchor-prefix "L"
                      :defaults                  {:doc/format :markdown}}

  :main ^:skip-aot com.defold.editor.Start

  :profiles          {:test    {:injections [(defonce force-toolkit-init (javafx.embed.swing.JFXPanel.))]}
                      :uberjar {:prep-tasks   ["clean" "protobuf" "javac" ["run" "-m" " aot"] "compile"]
                                :jvm-opts     ["-Dclojure.compiler.direct-linking=true"]
                                :aot          :all
                                :source-paths ["sidecar"]}
                      :dev     {:dependencies      [[org.clojure/test.check   "0.9.0"]
                                                    [org.mockito/mockito-core "1.10.19"]
                                                    [org.clojure/tools.trace  "0.7.9"]
                                                    [criterium "0.4.3"]
                                                    [ring "1.4.0"]]
                                :repl-options      {:init-ns user}
                                :proto-paths       ["test/proto"]
                                :java-source-paths ["dev/java"]
                                :source-paths      ["dev/clj"]
                                :resource-paths    ["test/resources"]}})

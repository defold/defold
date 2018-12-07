(defproject defold-editor "2.0.0-SNAPSHOT"
  :description      "Defold game editor"
  :url              "https://www.defold.com/learn/"

  :repositories     {"local" ~(str (.toURI (java.io.File. "localjars")))}

  :plugins          [[lein-protobuf-minimal "0.4.4" :hooks false]
                     [lein-sass "0.4.0"]
                     [codox "0.9.3"]]

  :dependencies     [[org.clojure/clojure                         "1.8.0"]
                     [org.clojure/core.cache                      "0.6.5"]
                     [org.clojure/tools.cli                       "0.3.5"]
                     [org.clojure/tools.macro                     "0.1.5"]
                     [org.clojure/tools.namespace                 "0.2.10"]
                     [org.clojure/data.int-map                    "0.2.4"]
                     [org.clojure/data.json                       "0.2.6"]
                     [com.cognitect/transit-clj                   "0.8.285"
                      :exclusions [com.fasterxml.jackson.core/jackson-core]] ; transit-clj -> 2.3.2, amazonica -> 2.6.6
                     [prismatic/schema                            "1.0.4"]
                     [prismatic/plumbing                          "0.5.2"]
                     [com.google.protobuf/protobuf-java           "2.3.0"]
                     [ch.qos.logback/logback-classic              "1.2.1"]
                     [org.slf4j/jul-to-slf4j                      "1.7.22"]
                     [joda-time/joda-time                         "2.9.2"]
                     [commons-io/commons-io                       "2.4"]
                     [commons-configuration/commons-configuration "1.10"
                      :exclusions [commons-logging/commons-logging]] ; commons-configuration -> 1.1.1, amazonica -> 1.2
                     [commons-codec/commons-codec                 "1.10"]
                     [org.projectodd.shimdandy/shimdandy-api      "1.2.0"]
                     [org.projectodd.shimdandy/shimdandy-impl     "1.2.0"]
                     [potemkin                                    "0.4.3"]
                     [com.nanohttpd/nanohttpd                     "2.1.1"]
                     [com.sun.jersey/jersey-core                  "1.19"]
                     [com.sun.jersey/jersey-client                "1.19"]
                     [com.sun.jersey.contribs/jersey-multipart    "1.19"]
                     [javax.vecmath/vecmath                       "1.5.2"]
                     [org.codehaus.jackson/jackson-core-asl       "1.9.13"]
                     [org.codehaus.jackson/jackson-mapper-asl     "1.9.13"]
                     [org.eclipse.jgit/org.eclipse.jgit           "4.2.0.201601211800-r"
                      :exclusions [org.apache.httpcomponents/httpclient]] ; jgit -> 4.3.6, amazonica -> 4.5.2
                     [clj-antlr                                   "0.2.2"]
                     [org.apache.commons/commons-compress         "1.18"]

                     [net.java.dev.jna/jna                        "4.1.0"]
                     [net.java.dev.jna/jna-platform               "4.1.0"]

                     [com.defold.lib/bob                          "1.0"]
                     [com.defold.lib/openmali                     "1.0"]

                     [com.atlassian.commonmark/commonmark         "0.9.0"]

                     [amazonica                                   "0.3.79"
                      :exclusions [com.amazonaws/aws-java-sdk com.amazonaws/amazon-kinesis-client]]
                     [com.amazonaws/aws-java-sdk-core             "1.11.63"]
                     [com.amazonaws/aws-java-sdk-s3               "1.11.63"]

                     [org.jogamp.gluegen/gluegen-rt               "2.3.2"]
                     [org.jogamp.gluegen/gluegen-rt               "2.3.2" :classifier "natives-linux-amd64"]
                     [org.jogamp.gluegen/gluegen-rt               "2.3.2" :classifier "natives-macosx-universal"]
                     [org.jogamp.gluegen/gluegen-rt               "2.3.2" :classifier "natives-windows-amd64"]
                     [org.jogamp.gluegen/gluegen-rt               "2.3.2" :classifier "natives-windows-i586"]
                     [org.jogamp.jogl/jogl-all                    "2.3.2"]
                     [org.jogamp.jogl/jogl-all                    "2.3.2" :classifier "natives-linux-amd64"]
                     [org.jogamp.jogl/jogl-all                    "2.3.2" :classifier "natives-macosx-universal"]
                     [org.jogamp.jogl/jogl-all                    "2.3.2" :classifier "natives-windows-amd64"]
                     [org.jogamp.jogl/jogl-all                    "2.3.2" :classifier "natives-windows-i586"]]

  :source-paths      ["src/clj"]

  :java-source-paths ["src/java"]

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
                      "../engine/rig/proto"
                      "../engine/script/src"]

  :protobuf-includes ["../engine/gameobject/proto"
                      "../engine/gamesys/proto"
                      "../engine/ddf/src"
                      "../engine/script/src"
                      ~(str (System/getenv "DYNAMO_HOME") "/ext/include")]

  :protobuf-exclude  ["../engine/ddf/src/test"]

  :sass              {:src "styling/stylesheets/main.sass"
                      :output-directory "resources/editor.css"
                      :source-maps false}

  :aliases           {"benchmark"     ["with-profile" "+test" "trampoline" "run" "-m" "benchmark.graph-benchmark"]
                      "lint" ["with-profile" "+test" "eastwood"]}

  ;; used by `pack` task
  :packing           {:pack-path "resources/_unpack"}

  :codox             {:sources                   ["src/clj"]
                      :output-dir                "target/doc/api"
                      :exclude                   [internal]
                      :src-dir-uri               "http://github.com/relevance/defold/blob/clojure-sceneed"
                      :src-linenum-anchor-prefix "L"
                      :defaults                  {:doc/format :markdown}}

  :jvm-opts          ["-Djna.nosys=true" "-Djava.net.preferIPv4Stack=true"]
  :main ^:skip-aot   com.defold.editor.Start

  :uberjar-exclusions [#"^natives/"]

  :profiles          {:test    {:injections [(defonce force-toolkit-init (javafx.embed.swing.JFXPanel.))]
                                :resource-paths ["test/resources"]}
                      :uberjar {:prep-tasks  ^:replace ["clean" "protobuf" ["sass" "once"] "javac" ["run" "-m" "aot"]]
                                :aot          :all
                                :omit-source  true
                                :source-paths ["sidecar"]}
                      :release {:jvm-opts          ["-Ddefold.build=release"]}
                      :dev     {;;:plugins           [[jonase/eastwood "0.3.5" :exclusions [org.clojure/clojure]]]
                                :dependencies      [[org.clojure/test.check   "0.9.0"]
                                                    [org.mockito/mockito-core "1.10.19"]
                                                    [org.clojure/tools.trace  "0.7.9"]
                                                    [com.clojure-goes-fast/clj-async-profiler "0.3.0"]
                                                    [criterium "0.4.3"]
                                                    [org.mockito/mockito-core "1.10.19"]
                                                    [ring "1.4.0"]]
                                :repl-options      {:init-ns user}
                                :proto-paths       ["test/proto"]
                                :resource-paths    ["test/resources"]
                                :jvm-opts          ["-Ddefold.unpack.path=tmp/unpack"
                                                    "-Ddefold.log.dir="
                                                    "-Djogl.debug.DebugGL" ; TraceGL is also useful
                                                    "-Djogl.texture.notexrect=true"
                                                    ;;"-XX:+UnlockCommercialFeatures"
                                                    ;;"-XX:+FlightRecorder"
                                                    "-XX:-OmitStackTraceInFastThrow"]}}

  :eastwood {:continue-on-exception true
             :exclude-namespaces [
                                  editor.code.view ; causes Method code too large! transitively in namespaces below, typically via editor.app-view
                                  editor.console
                                  editor.debug-view
                                  editor.app-view
                                  editor.game-object
                                  editor.curve-view
                                  editor.atlas

                                  editor.cubemap
                                  editor.font
                                  editor.particlefx
                                  editor.gui
                                  editor.collection
                                  editor.collision-object
                                  editor.tile-source
                                  editor.tile-map
                                  editor.resource-types
                                  editor.asset-browser
                                  editor.outline-view
                                  editor.boot-open-project
                                  editor.boot

                                  dev

                                  clojure.data.int-map-fixed

                                  integration.test-util
                                  integration.hot-reload-test
                                  integration.collada-test
                                  integration.build-test
                                  integration.resource-watch-test
                                  integration.spine-test
                                  integration.undo-test
                                  integration.collection-test
                                  integration.scope-test
                                  integration.collection-proxy-test
                                  integration.game-object-test
                                  integration.animation-set-test
                                  integration.engine.native-extensions-test
                                  integration.save-test
                                  integration.sound-test
                                  integration.scene-test
                                  integration.particlefx-test
                                  integration.subselection-test
                                  integration.protobuf-types-test
                                  integration.script-test
                                  integration.collision-object-test
                                  integration.atlas-test
                                  integration.game-project-test
                                  integration.sprite-test
                                  integration.font-test
                                  integration.tex-packing-test
                                  integration.tile-map-test
                                  integration.model-test
                                  integration.collada-scene-test
                                  integration.tile-source-test
                                  integration.asset-browser-test
                                  integration.library-test
                                  integration.display-profiles-test
                                  integration.reload-test
                                  integration.material-test
                                  integration.factory-test
                                  integration.test-util-test
                                  integration.label-test
                                  integration.gui-clipping-test
                                  integration.build-errors-test
                                  integration.script-properties-test
                                  integration.curve-view-test
                                  integration.app-view-test
                                  integration.gui-test
                                  integration.outline-test
                                  integration.perf-test

                                  dynamo.integration.override-test


                                  editor.console-test
                                  editor.handler-test
                                  editor.fs-test
                                  editor.sync-test
                                  editor.gviz-test
                                  editor.defold-project-search-test
                                  editor.targets-test
                                  editor.defold-project-test
                                  editor.pipeline-test
                                  editor.code.util-test


                                  benchmark.graph-benchmark

                                  internal.util-test]})


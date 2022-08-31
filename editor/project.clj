;; Copyright 2020-2022 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(defproject defold-editor "2.0.0-SNAPSHOT"
  :description      "Defold game editor"
  :url              "https://www.defold.com/learn/"

  :repositories     {"local" ~(str (.toURI (java.io.File. "localjars")))}

  :plugins          [[lein-protobuf-minimal-mg "0.4.5" :hooks false]
                     [lein-sass "0.4.0"]
                     [codox "0.9.3"]]

  :dependencies     [[org.clojure/clojure                         "1.10.0"]
                     [org.clojure/core.cache                      "0.7.1"]
                     [org.clojure/tools.cli                       "0.3.5"]
                     [org.clojure/tools.macro                     "0.1.5"]
                     [org.clojure/tools.namespace                 "1.2.0"]
                     [org.clojure/data.int-map                    "0.2.4"]
                     [org.clojure/data.json                       "0.2.6"]
                     [com.cognitect/transit-clj                   "0.8.285"
                      :exclusions [com.fasterxml.jackson.core/jackson-core]] ; transit-clj -> 2.3.2, amazonica -> 2.6.6
                     [prismatic/schema                            "1.1.9"]
                     [prismatic/plumbing                          "0.5.2"]
                     [com.google.protobuf/protobuf-java           "3.20.1"]
                     [ch.qos.logback/logback-classic              "1.2.1"]
                     [org.slf4j/jul-to-slf4j                      "1.7.22"]
                     [joda-time/joda-time                         "2.9.2"]
                     [commons-io/commons-io                       "2.4"]
                     [org.apache.commons/commons-configuration2   "2.0"]
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
                     [clj-antlr                                   "0.2.2"
                      :exclusions [org.antlr/antlr4 org.antlr/antlr4-runtime]]
                     [org.antlr/antlr4 "4.7.2"]
                     [org.antlr/antlr4-runtime "4.7.2"]
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

                     ;; bob needs javax.xml.bind, and it's removed in jdk 11
                     [javax.xml.bind/jaxb-api "2.3.0"]
                     [com.sun.xml.bind/jaxb-core "2.3.0"]
                     [com.sun.xml.bind/jaxb-impl "2.3.0"]

                     [org.luaj/luaj-jse "3.0.1"]

                     [cljfx "1.7.21"]

                     [org.openjfx/javafx-base "18"]
                     [org.openjfx/javafx-base "18" :classifier "linux"]
                     [org.openjfx/javafx-base "18" :classifier "mac"]
                     [org.openjfx/javafx-base "18" :classifier "win"]
                     [org.openjfx/javafx-controls "18"]
                     [org.openjfx/javafx-controls "18" :classifier "linux"]
                     [org.openjfx/javafx-controls "18" :classifier "mac"]
                     [org.openjfx/javafx-controls "18" :classifier "win"]
                     [org.openjfx/javafx-graphics "18"]
                     [org.openjfx/javafx-graphics "18" :classifier "linux"]
                     [org.openjfx/javafx-graphics "18" :classifier "mac"]
                     [org.openjfx/javafx-graphics "18" :classifier "win"]
                     [org.openjfx/javafx-media "18"]
                     [org.openjfx/javafx-media "18" :classifier "linux"]
                     [org.openjfx/javafx-media "18" :classifier "mac"]
                     [org.openjfx/javafx-media "18" :classifier "win"]
                     [org.openjfx/javafx-web "18"]
                     [org.openjfx/javafx-web "18" :classifier "linux"]
                     [org.openjfx/javafx-web "18" :classifier "mac"]
                     [org.openjfx/javafx-web "18" :classifier "win"]
                     [org.openjfx/javafx-fxml "18"]
                     [org.openjfx/javafx-fxml "18" :classifier "linux"]
                     [org.openjfx/javafx-fxml "18" :classifier "mac"]
                     [org.openjfx/javafx-fxml "18" :classifier "win"]
                     [org.openjfx/javafx-swing "18"]
                     [org.openjfx/javafx-swing "18" :classifier "linux"]
                     [org.openjfx/javafx-swing "18" :classifier "mac"]
                     [org.openjfx/javafx-swing "18" :classifier "win"]

                     [com.metsci.ext.org.jogamp.gluegen/gluegen-rt "2.4.0-rc-20200202"]
                     [com.metsci.ext.org.jogamp.gluegen/gluegen-rt "2.4.0-rc-20200202" :classifier "natives-linux-amd64"]
                     [com.metsci.ext.org.jogamp.gluegen/gluegen-rt "2.4.0-rc-20200202" :classifier "natives-macosx-universal"]
                     [com.metsci.ext.org.jogamp.gluegen/gluegen-rt "2.4.0-rc-20200202" :classifier "natives-windows-amd64"]
                     [com.metsci.ext.org.jogamp.gluegen/gluegen-rt "2.4.0-rc-20200202" :classifier "natives-windows-i586"]
                     [com.metsci.ext.org.jogamp.jogl/jogl-all      "2.4.0-rc-20200202"]
                     [com.metsci.ext.org.jogamp.jogl/jogl-all      "2.4.0-rc-20200202" :classifier "natives-linux-amd64"]
                     [com.metsci.ext.org.jogamp.jogl/jogl-all      "2.4.0-rc-20200202" :classifier "natives-macosx-universal"]
                     [com.metsci.ext.org.jogamp.jogl/jogl-all      "2.4.0-rc-20200202" :classifier "natives-windows-amd64"]
                     [com.metsci.ext.org.jogamp.jogl/jogl-all      "2.4.0-rc-20200202" :classifier "natives-windows-i586"]

                     [org.snakeyaml/snakeyaml-engine "1.0"]]

  :source-paths      ["src/clj"]

  :test-paths        ["test"]

  :java-source-paths ["src/java"]

  :resource-paths    ["resources" "generated-resources"]

  :proto-paths       ["../com.dynamo.cr/com.dynamo.cr.common/proto"
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

  :sass              {:src "styling/stylesheets/"
                      :output-directory "resources/"
                      :source-maps false}

  :aliases           {"benchmark" ["with-profile" "+test" "trampoline" "run" "-m" "benchmark.graph-benchmark"]
                      "preflight" ["with-profile" "+preflight,+dev,+test" "preflight"]}

  ;; used by `pack` task
  :packing           {:pack-path "resources/_unpack"}

  :codox             {:sources                   ["src/clj"]
                      :output-dir                "target/doc/api"
                      :exclude                   [internal]
                      :src-dir-uri               "http://github.com/relevance/defold/blob/clojure-sceneed"
                      :src-linenum-anchor-prefix "L"
                      :defaults                  {:doc/format :markdown}}

  :jvm-opts          ["-Djna.nosys=true"
                      "-Djava.net.preferIPv4Stack=true"
                      "-Dfile.encoding=UTF-8"
                      "--illegal-access=warn"
                      ;; hide warnings about illegal reflective access by jogl
                      "--add-opens=java.base/java.lang=ALL-UNNAMED"
                      "--add-opens=java.desktop/sun.awt=ALL-UNNAMED"
                      "--add-opens=java.desktop/sun.java2d.opengl=ALL-UNNAMED"
                      ;; hide warnings about illegal reflective access by clojure
                      "--add-opens=java.xml/com.sun.org.apache.xerces.internal.jaxp=ALL-UNNAMED"]
  :main ^:skip-aot   com.defold.editor.Main

  :uberjar-exclusions [#"^natives/"]

  :profiles          {:test    {:injections [(defonce initialize-test-prerequisites
                                               (do
                                                 (com.defold.libs.ResourceUnpacker/unpackResources)
                                                 (javafx.application.Platform/startup
                                                   (fn []
                                                     (com.jogamp.opengl.GLProfile/initSingleton)))))]
                                :resource-paths ["test/resources"]
                                :jvm-opts ["-Ddefold.tests=true"]}
                      :preflight {:dependencies [[jonase/kibit "0.1.6" :exclusions [org.clojure/clojure]]
                                                 [cljfmt-mg "0.6.4" :exclusions [org.clojure/clojure]]]}
                      :uberjar {:prep-tasks  ^:replace ["clean" "protobuf" ["sass" "once"] "javac" ["run" "-m" "aot"]]
                                :aot          :all
                                :omit-source  true
                                :source-paths ["sidecar"]}
                      :local-repl {:injections [(future ((requiring-resolve 'editor/-main)))]
                                   :jvm-opts ["-Ddefold.nrepl=false"]}
                      :vscode {:plugins [[nrepl "0.6.0"]]}
                      :cider {:plugins [[cider/cider-nrepl "0.24.0"]
                                        ;;[refactor-nrepl "2.4.0"] ;; -- this does not seem to work well together with cider-nrepl 0.24.0 so it might be better to just skip.
                                        [com.billpiel/sayid "0.0.18"]]}
                      :release {:jvm-opts          ["-Ddefold.build=release"]}
                      :headless {:jvm-opts ["-Dtestfx.robot=glass" "-Dglass.platform=Monocle" "-Dmonocle.platform=Headless" "-Dprism.order=sw"]
                                 :dependencies [[org.testfx/openjfx-monocle "jdk-12.0.1+2"]]}
                      :smoke-test {:jvm-opts ["-Ddefold.smoke.log=true"]}
                      :reveal {:source-paths ["src/reveal"]
                               :injections [(require 'editor.reveal)]
                               :dependencies [[vlaaad/reveal "1.3.273"]]}
                      :metrics {:jvm-opts ["-Ddefold.metrics=true"]}
                      :dev     {:dependencies      [[com.clojure-goes-fast/clj-async-profiler "0.5.1"]
                                                    [com.clojure-goes-fast/clj-memory-meter "0.1.2"]
                                                    [criterium "0.4.3"]
                                                    [org.clojure/test.check   "0.9.0"]
                                                    [org.clojure/tools.trace  "0.7.9"]
                                                    [org.mockito/mockito-core "1.10.19"]
                                                    [ring "1.4.0"]]
                                :repl-options      {:init-ns user}
                                :proto-paths       ["test/proto"]
                                :resource-paths    ["test/resources"]
                                :jvm-opts          ["-Ddefold.unpack.path=tmp/unpack"
                                                    "-Ddefold.nrepl=true"
                                                    "-Ddefold.log.dir="
                                                    "-Djogl.debug.DebugGL" ; TraceGL is also useful
                                                    "-Djogl.texture.notexrect=true"
                                                    "-XX:MaxRAMPercentage=75"
                                                    ;"-XX:+UnlockCommercialFeatures"
                                                    ;"-XX:+FlightRecorder"
                                                    "-XX:-OmitStackTraceInFastThrow"

                                                    ;; Flags for async-profiler.
                                                    ;; From https://github.com/clojure-goes-fast/clj-async-profiler/blob/master/README.md
                                                    "-Djdk.attach.allowAttachSelf"   ; Required for attach to running process.
                                                    "-XX:+UnlockDiagnosticVMOptions" ; Required for DebugNonSafepoints.
                                                    "-XX:+DebugNonSafepoints"]}})     ; Without this, there is a high chance that simple inlined methods will not appear in the profile.

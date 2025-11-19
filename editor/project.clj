;; Copyright 2020-2025 The Defold Foundation
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

  :repositories     {"local" ~(str (.toURI (java.io.File. "localjars")))
                     "jogamp" "https://jogamp.org/deployment/maven"}

  :plugins          [[lein-protobuf-minimal-mg "0.4.5" :hooks false]
                     [codox "0.9.3"]]

  :dependencies     [[org.clojure/clojure                         "1.12.0"]
                     [org.clojure/core.cache                      "0.7.1"]
                     [org.clojure/core.async                      "1.5.648"]
                     [org.clojure/tools.cli                       "0.3.5"]
                     [org.clojure/tools.macro                     "0.1.5"]
                     [org.clojure/tools.namespace                 "1.2.0"]
                     [org.clojure/data.int-map                    "1.3.0"]
                     [org.clojure/data.json                       "2.5.0"]
                     [com.cognitect/transit-clj                   "0.8.285"]
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
                     [org.eclipse.jgit/org.eclipse.jgit           "4.2.0.201601211800-r"]
                     [org.apache.commons/commons-compress         "1.18"]

                     [net.java.dev.jna/jna                        "4.1.0"]
                     [net.java.dev.jna/jna-platform               "4.1.0"]

                     [com.defold.lib/bob                          "1.0"]
                     [com.defold.lib/openmali                     "1.0"]
                     [com.defold.lib/icu4j                        "1.0"]

                     [metosin/reitit-core "0.8.0-alpha1"]

                     [org.commonmark/commonmark "0.25.1"]
                     [org.commonmark/commonmark-ext-autolink "0.25.1"]
                     [org.commonmark/commonmark-ext-gfm-tables "0.25.1"]
                     [org.commonmark/commonmark-ext-yaml-front-matter "0.25.1"]
                     [org.commonmark/commonmark-ext-heading-anchor "0.25.1"]

                     [com.cognitect.aws/api "0.8.673"]
                     [com.cognitect.aws/endpoints "1.1.12.478"]
                     [com.cognitect.aws/s3 "847.2.1387.0"]

                     ;; bob needs javax.xml.bind, and it's removed in jdk 11
                     [javax.xml.bind/jaxb-api "2.3.0"]
                     [com.sun.xml.bind/jaxb-core "2.3.0"]
                     [com.sun.xml.bind/jaxb-impl "2.3.0"]

                     [org.luaj/luaj-jse "3.0.1"]

                     [com.github.ben-manes.caffeine/caffeine "3.1.2"]

                     [cljfx "1.9.6"
                      :exclusions [org.clojure/clojure
                                   org.openjfx/javafx-base
                                   org.openjfx/javafx-graphics
                                   org.openjfx/javafx-controls
                                   org.openjfx/javafx-media
                                   org.openjfx/javafx-web]]

                     [org.jogamp.gluegen/gluegen-rt "2.6.0"]
                     [org.jogamp.gluegen/gluegen-rt "2.6.0" :classifier "natives-linux-amd64"]
                     [org.jogamp.gluegen/gluegen-rt "2.6.0" :classifier "natives-macosx-universal"]
                     [org.jogamp.gluegen/gluegen-rt "2.6.0" :classifier "natives-windows-amd64"]
                     [org.jogamp.jogl/jogl-all      "2.6.0"]
                     [org.jogamp.jogl/jogl-all      "2.6.0" :classifier "natives-linux-amd64"]
                     [org.jogamp.jogl/jogl-all      "2.6.0" :classifier "natives-macosx-universal"]
                     [org.jogamp.jogl/jogl-all      "2.6.0" :classifier "natives-windows-amd64"]

                     [org.snakeyaml/snakeyaml-engine "1.0"]

                     [net.objecthunter/exp4j "0.4.8"]]

  :icu4j {:version "77.1"}

  :source-paths      ["src/clj"]

  :test-paths        ["test"]

  :java-source-paths ["src/java" "src/antlr" "src/common"]

  :resource-paths    ["resources" "generated-resources"]

  :proto-paths       ["../com.dynamo.cr/com.dynamo.cr.common/proto"
                      "../engine/ddf/src"
                      "../engine/engine/proto"
                      "../engine/gameobject/proto"
                      "../engine/gamesys/proto"
                      "../engine/graphics/proto"
                      "../engine/input/proto"
                      "../engine/particle/proto/particle"
                      "../engine/render/proto"
                      "../engine/resource/proto"
                      "../engine/rig/proto"
                      "../engine/script/src"]

  :protobuf-includes ["../engine/gameobject/proto"
                      "../engine/gamesys/proto"
                      "../engine/graphics/proto"
                      "../engine/ddf/src"
                      "../engine/script/src"
                      ~(str (System/getenv "DYNAMO_HOME") "/ext/include")]

  :protobuf-exclude  ["../engine/ddf/src/test"]

  :sass              {:src "styling/stylesheets/"
                      :output-directory "resources/"
                      :source-maps false}

  :aliases           {"benchmark" ["with-profile" "+test" "trampoline" "run" "-m" "benchmark.graph-benchmark"]
                      "preflight" ["with-profile" "+preflight,+dev,+test" "preflight"]
                      "prerelease" ["do" "clean," "protobuf," "sass" "once," "javac," "with-profile" "dev,sidecar,release" "run" "-m" "aot"]}

  ;; used by `pack` task
  :packing           {:pack-path "resources/_unpack"
                      :lua-language-server-version "v1.7795"}

  :codox             {:sources                   ["src/clj"]
                      :output-dir                "target/doc/api"
                      :exclude                   [internal]
                      :src-dir-uri               "http://github.com/relevance/defold/blob/clojure-sceneed"
                      :src-linenum-anchor-prefix "L"
                      :defaults                  {:doc/format :markdown}}

  :jvm-opts          ["-Djna.nosys=true"
                      "-Djava.net.preferIPv4Stack=true"
                      "-Dfile.encoding=UTF-8"
                      ;; hide warnings about illegal reflective access by jogl
                      "--add-opens=java.base/java.lang=ALL-UNNAMED"
                      "--add-opens=java.desktop/sun.awt=ALL-UNNAMED"
                      "--add-opens=java.desktop/sun.java2d.opengl=ALL-UNNAMED"
                      ;; used in lsp.clj (java.lang.String sun.nio.fs.Globs.toUnixRegexPattern)
                      "--add-opens=java.base/sun.nio.fs=ALL-UNNAMED"
                      ;; hide warnings about illegal reflective access by clojure
                      "--add-opens=java.xml/com.sun.org.apache.xerces.internal.jaxp=ALL-UNNAMED"
                      ;; used in editor.scene$read_to_buffered_image
                      "--add-opens=java.desktop/sun.awt.image=ALL-UNNAMED"
                      "--enable-native-access=ALL-UNNAMED"
                      "-XX:+UseCompactObjectHeaders"]
                      ;; "-XX:MaxJavaStackTraceDepth=1073741823"

  :main ^:skip-aot   com.defold.editor.Main

  :uberjar-exclusions [#"^natives/"]

  :javac-options ["-Xlint:unchecked" "-Xlint:deprecation"]

  ;; Skip native extensions tests:
  ;; lein test :no-native-extensions
  :test-selectors {:no-native-extensions (complement :native-extensions)}

  :prep-tasks [["with-profile" "antlr" "run" "-m" "org.antlr.v4.Tool"
                "LSPSnippet.g4"
                "-o" "src/antlr/com/defold/editor"
                "-package" "com.defold.editor"
                "-no-listener"
                "-no-visitor"]
               "javac"
               "compile"]

  :clean-targets ^{:protect false} [:target-path "src/antlr"]

  :profiles          {:test    {:injections [(com.defold.libs.ResourceUnpacker/unpackResources)]
                                :resource-paths ["test/resources"]
                                :jvm-opts ["-Ddefold.tests=true"
                                           "-Ddefold.cache.libraries=true"]}
                      :preflight {:dependencies [[jonase/kibit "0.1.6" :exclusions [org.clojure/clojure]]
                                                 [cljfmt-mg "0.6.4" :exclusions [org.clojure/clojure]]]}
                      :sidecar {:source-paths ["sidecar"]}
                      :docs {:source-paths ["src/docs"]
                             ;; Docs are generated by the engine build scripts at a point BEFORE
                             ;; the bob is even built, so we need a separate "project" without bob
                             ;; dependency to generate docs
                             :java-source-paths ^:replace ["src/antlr" "src/common"]
                             :dependencies ^:replace [[org.clojure/clojure "1.12.0"]
                                                      [commons-io/commons-io "2.4"]
                                                      [prismatic/schema "1.1.9"]
                                                      [org.luaj/luaj-jse "3.0.1"]
                                                      ;; normally, we get this from bob,
                                                      ;; but docs are built before bob
                                                      [org.antlr/antlr4-runtime "4.9.1"]]}
                      :antlr {:prep-tasks ^:replace []
                              :java-source-paths ^:replace []
                              :dependencies ^:replace [[org.clojure/clojure "1.12.0"]
                                                       [org.antlr/antlr4 "4.9.1"]]}
                      :uberjar {:prep-tasks  ^:replace []
                                :aot          :all
                                :auto-clean   false
                                :omit-source  true}
                      :local-repl {:injections [(require 'dev) (future ((requiring-resolve 'editor/-main)))]
                                   :jvm-opts ["-Ddefold.nrepl=false"]}
                      :vscode {:plugins [[nrepl "0.6.0"]]}
                      :cider {:plugins [[cider/cider-nrepl "0.24.0"]
                                        ;;[refactor-nrepl "2.4.0"] ;; -- this does not seem to work well together with cider-nrepl 0.24.0 so it might be better to just skip.
                                        [com.billpiel/sayid "0.0.18"]]}
                      :release {:jvm-opts ["-Ddefold.build=release" "-Dclojure.spec.compile-asserts=false"]}
                      :headless {:jvm-opts ["-Dtestfx.robot=glass" "-Dglass.platform=Monocle" "-Dmonocle.platform=Headless" "-Dprism.order=sw"]
                                 :dependencies [[org.testfx/openjfx-monocle "jdk-12.0.1+2"]]}
                      :smoke-test {:jvm-opts ["-Ddefold.smoke.log=true"]}
                      :cache-libraries {:jvm-opts ["-Ddefold.cache.libraries=true"]}
                      :portal {:source-paths ["src/portal"]
                               :dependencies [[djblue/portal "0.60.2"]]}
                      :reveal {:source-paths ["src/reveal"]
                               :jvm-opts ["-Djol.magicFieldOffset=true" "-XX:+EnableDynamicAgentLoading"]
                               :injections [(require 'editor.reveal)]
                               :dependencies [[vlaaad/reveal "1.3.312"]
                                              [org.openjfx/javafx-web "25"]]}
                      :metrics {:jvm-opts ["-Ddefold.metrics=true"]}
                      :jamm {:dependencies [[com.github.jbellis/jamm "0.4.0"]]
                             :jvm-opts [~(str "-javaagent:"
                                           (.replace (System/getProperty "user.home") \\ \/)
                                           "/.m2/repository/com/github/jbellis/jamm/0.4.0/jamm-0.4.0.jar")
                                        "-Ddefold.jamm=true"
                                        "--add-opens=java.base/java.util=ALL-UNNAMED"
                                        "--add-opens=java.base/java.util.function=ALL-UNNAMED"
                                        "--add-opens=java.base/java.util.regex=ALL-UNNAMED"
                                        "--add-opens=java.base/sun.nio.ch=ALL-UNNAMED"
                                        "--add-opens=java.net.http/jdk.internal.net.http=ALL-UNNAMED"
                                        "--add-opens=java.net.http/jdk.internal.net.http.common=ALL-UNNAMED"]}
                      :strict-pb-map-keys {:jvm-opts ["-Ddefold.protobuf.strict.enable=true"]}
                      :no-asserts {:global-vars {*assert* false}}
                      :no-decorated-exceptions {:jvm-opts ["-Ddefold.exception.decorate.disable=true"]}
                      :no-schemas {:jvm-opts ["-Ddefold.schema.check.disable=true"]}
                      :no-spec-asserts {:jvm-opts ["-Dclojure.spec.compile-asserts=false"]}
                      :performance [:no-decorated-exceptions :no-schemas :no-spec-asserts]
                      :16gb {:jvm-opts ["-Xmx16g"]}
                      :x86_64-linux {:dependencies [[org.openjfx/javafx-base "25" :classifier "linux" :exclusions [org.openjfx/javafx-base]]
                                                    [org.openjfx/javafx-controls "25" :classifier "linux" :exclusions [org.openjfx/javafx-controls org.openjfx/javafx-graphics]]
                                                    [org.openjfx/javafx-graphics "25" :classifier "linux" :exclusions [org.openjfx/javafx-graphics org.openjfx/javafx-base]]
                                                    [org.openjfx/javafx-media "25" :classifier "linux" :exclusions [org.openjfx/javafx-media org.openjfx/javafx-graphics]]
                                                    [org.openjfx/javafx-fxml "25" :classifier "linux" :exclusions [org.openjfx/javafx-fxml org.openjfx/javafx-controls]]
                                                    [org.openjfx/javafx-swing "25" :classifier "linux" :exclusions [org.openjfx/javafx-swing org.openjfx/javafx-graphics]]]
                                     :uberjar-name "editor-x86_64-linux-standalone.jar"}
                      :x86_64-win32 {:dependencies [[org.openjfx/javafx-base "25" :classifier "win" :exclusions [org.openjfx/javafx-base]]
                                                    [org.openjfx/javafx-controls "25" :classifier "win" :exclusions [org.openjfx/javafx-controls org.openjfx/javafx-graphics]]
                                                    [org.openjfx/javafx-graphics "25" :classifier "win" :exclusions [org.openjfx/javafx-graphics org.openjfx/javafx-base]]
                                                    [org.openjfx/javafx-media "25" :classifier "win" :exclusions [org.openjfx/javafx-media org.openjfx/javafx-graphics]]
                                                    [org.openjfx/javafx-fxml "25" :classifier "win" :exclusions [org.openjfx/javafx-fxml org.openjfx/javafx-controls]]
                                                    [org.openjfx/javafx-swing "25" :classifier "win" :exclusions [org.openjfx/javafx-swing org.openjfx/javafx-graphics]]]
                                     :uberjar-name "editor-x86_64-win32-standalone.jar"}
                      :x86_64-macos {:dependencies [[org.openjfx/javafx-base "25" :classifier "mac" :exclusions [org.openjfx/javafx-base]]
                                                    [org.openjfx/javafx-controls "25" :classifier "mac" :exclusions [org.openjfx/javafx-controls org.openjfx/javafx-graphics]]
                                                    [org.openjfx/javafx-graphics "25" :classifier "mac" :exclusions [org.openjfx/javafx-graphics org.openjfx/javafx-base]]
                                                    [org.openjfx/javafx-media "25" :classifier "mac" :exclusions [org.openjfx/javafx-media org.openjfx/javafx-graphics]]
                                                    [org.openjfx/javafx-fxml "25" :classifier "mac" :exclusions [org.openjfx/javafx-fxml org.openjfx/javafx-controls]]
                                                    [org.openjfx/javafx-swing "25" :classifier "mac" :exclusions [org.openjfx/javafx-swing org.openjfx/javafx-graphics]]]
                                     :uberjar-name "editor-x86_64-macos-standalone.jar"}
                      :arm64-macos {:dependencies [[org.openjfx/javafx-base "25" :classifier "mac-aarch64" :exclusions [org.openjfx/javafx-base]]
                                                   [org.openjfx/javafx-controls "25" :classifier "mac-aarch64" :exclusions [org.openjfx/javafx-controls org.openjfx/javafx-graphics]]
                                                   [org.openjfx/javafx-graphics "25" :classifier "mac-aarch64" :exclusions [org.openjfx/javafx-graphics org.openjfx/javafx-base]]
                                                   [org.openjfx/javafx-media "25" :classifier "mac-aarch64" :exclusions [org.openjfx/javafx-media org.openjfx/javafx-graphics]]
                                                   [org.openjfx/javafx-fxml "25" :classifier "mac-aarch64" :exclusions [org.openjfx/javafx-fxml org.openjfx/javafx-controls]]
                                                   [org.openjfx/javafx-swing "25" :classifier "mac-aarch64" :exclusions [org.openjfx/javafx-swing org.openjfx/javafx-graphics]]]
                                    :uberjar-name "editor-arm64-macos-standalone.jar"}
                      :dev     {:dependencies      [;; generic javafx dep picks up natives for the current platform
                                                    [org.openjfx/javafx-base "25"]
                                                    [org.openjfx/javafx-controls "25"]
                                                    [org.openjfx/javafx-graphics "25"]
                                                    [org.openjfx/javafx-media "25"]
                                                    [org.openjfx/javafx-fxml "25"]
                                                    [org.openjfx/javafx-swing "25"]
                                                    [com.clojure-goes-fast/clj-async-profiler "0.5.1"]
                                                    [criterium "0.4.3"]
                                                    [lambdaisland/deep-diff2 "2.10.211"]
                                                    [io.github.cljfx/dev "1.0.39"]
                                                    [org.clojure/test.check "1.1.1"]
                                                    [org.clojure/tools.trace "0.7.9"]
                                                    [org.mockito/mockito-core "1.10.19"]]
                                :source-paths      ["src/dev"]
                                :repl-options      {:init-ns user}
                                :proto-paths       ["test/proto"]
                                :resource-paths    ["test/resources"]
                                :jvm-opts          ["-Ddefold.extension.lua-preprocessor.url=https://github.com/defold/extension-lua-preprocessor/archive/refs/tags/1.1.3.zip"
                                                    "-Ddefold.extension.rive.url=https://github.com/defold/extension-rive/archive/refs/tags/8.5.0.zip"
                                                    "-Ddefold.extension.simpledata.url=https://github.com/defold/extension-simpledata/archive/refs/tags/v1.1.0.zip"
                                                    "-Ddefold.extension.spine.url=https://github.com/defold/extension-spine/archive/refs/tags/4.2.0.zip"
                                                    "-Ddefold.extension.teal.url=https://github.com/defold/extension-teal/archive/refs/tags/v1.4.zip"
                                                    "-Ddefold.extension.texturepacker.url=https://github.com/defold/extension-texturepacker/archive/refs/tags/2.5.0.zip"
                                                    "-Ddefold.unpack.path=tmp/unpack"
                                                    "-Ddefold.nrepl=true"
                                                    "-Ddefold.log.dir="
                                                    "-Ddefold.dev=true"
                                                    ;"-Djogl.verbose=true"
                                                    ;"-Djogl.debug=true"
                                                    "-Djogl.debug.DebugGL" ; TraceGL is also useful
                                                    "-Djogl.texture.notexrect=true"
                                                    "-XX:MaxRAMPercentage=75"
                                                    ;"-XX:+UnlockCommercialFeatures"
                                                    ;"-XX:+FlightRecorder"
                                                    "-XX:-OmitStackTraceInFastThrow"
                                                    "-Dclojure.spec.check-asserts=true"

                                                    ;; Flags for async-profiler.
                                                    ;; From https://github.com/clojure-goes-fast/clj-async-profiler/blob/master/README.md
                                                    "-Djdk.attach.allowAttachSelf"   ; Required for attach to running process.
                                                    "-XX:+UnlockDiagnosticVMOptions" ; Required for DebugNonSafepoints.
                                                    "-XX:+DebugNonSafepoints"]}})    ; Without this, there is a high chance that simple inlined methods will not appear in the profile.

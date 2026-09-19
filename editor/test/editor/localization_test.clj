(ns editor.localization-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [editor.code.lang.java-properties :as java-properties]
            [editor.fs :as fs]
            [editor.localization :as localization]
            [integration.test-util :as test-util]
            [internal.java :as java]
            [internal.util :as util]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.path :as path])
  (:import [java.io StringReader]
           [java.time LocalDate]
           [javafx.scene.control Label]))

(set! *warn-on-reflection* true)

(defn- bundle [locale->content]
  (coll/pair-map-by
    #(str (key %) ".editor_localization")
    #(fn [] (StringReader. (val %)))
    locale->content))

(defn- make
  ([]
   (make {}))
  ([locale->content]
   (localization/make
     (test-util/make-test-prefs)
     :test
     (bundle locale->content)
     #(throw %))))


(deftest pattern-test
  (testing "message pattern"
    (let [localization (make {"en" "hello = Hello, {name}!
                                    rename = Rename {count} {count, plural, one {file} other {files}}
                                    progress = It's {n, number, percent} done"})]
      (is (= "Hello, User!" (localization (localization/message "hello" {"name" "User"}))))
      (is (= "Rename 0 files" (localization (localization/message "rename" {"count" 0}))))
      (is (= "Rename 1 file" (localization (localization/message "rename" {"count" 1}))))
      (is (= "Rename 2 files" (localization (localization/message "rename" {"count" 2}))))
      (is (= "It's 65% done" (localization (localization/message "progress" {"n" 0.65}))))
      (testing "nesting"
        (is (= "Hello, Rename 1 file!" (localization (localization/message "hello" {"name" (localization/message "rename" {"count" 1})})))))))
  (testing "lists"
    (let [localization (make {"en" "apple = APPLE"})]
      (is (= "1" (localization (localization/and-list [1]))))
      (is (= "1" (localization (localization/or-list [1]))))
      (is (= "1 and 2" (localization (localization/and-list [1 2]))))
      (is (= "1 or 2" (localization (localization/or-list [1 2]))))
      (is (= "1, 2, and 3" (localization (localization/and-list [1 2 3]))))
      (is (= "1, 2, or 3" (localization (localization/or-list [1 2 3]))))
      (testing "nesting"
        (is (= "1, 2, and APPLE" (localization (localization/and-list [1 2 (localization/message "apple")]))))
        (is (= "1, 2, or APPLE" (localization (localization/or-list [1 2 (localization/message "apple")])))))))
  (testing "date"
    (let [localization (make)]
      (is (= "8/5/25" (localization (localization/date (LocalDate/of 2025 8 5)))))))
  (testing "join"
    (let [localization (make {"en" "arrow = ->\n a = apple"})]
      (is (= "a->b->c" (localization (localization/join "->" ["a" "b" "c"]))))
      (is (= "a->b->c" (localization (localization/join (localization/message "arrow") ["a" "b" "c"]))))
      (is (= "apple->b->c" (localization (localization/join "->" [(localization/message "a") "b" "c"]))))))
  (testing "transform"
    (let [localization (make {"en" "apple = APPLE"})]
      (is (= "=huh=" (localization (localization/transform "huh" #(str "=" % "=")))))
      (is (= "=APPLE=" (localization (localization/transform (localization/message "apple") #(str "=" % "="))))))))

(deftest model-property-message-test
  (is (= "glTF or GLB resource containing the model's visible geometry. Morph targets are imported together with the mesh."
         (test-util/localization (localization/message "property.model.mesh.tooltip"))))
  (is (= "Mesh"
         (test-util/localization (localization/message "property.model.mesh-name"))))
  (is (= "Optional named raw mesh from the selected scene. Leave empty to render the whole scene. A selected mesh is rendered once in mesh-local coordinates without glTF node transforms."
         (test-util/localization (localization/message "property.model.mesh-name.tooltip"))))
  (is (= "glTF or GLB resource containing the bone hierarchy used for skeletal animation. Defold imports the first skin in the file; this is often the same resource used for **Scene**."
         (test-util/localization (localization/message "property.model.skeleton.tooltip"))))
  (is (= "Mesh \"Tree\" no longer exists or is ambiguous in the selected scene."
         (test-util/localization (localization/message "error.collision-object-mesh-shape-mesh-missing" {"mesh" "Tree"})))))

(deftest available-locales-test
  (is (= [] (localization/available-locales @(make))))
  (is (= ["en"] (localization/available-locales @(make {"en" ""}))))
  (is (= ["en" "sv"] (localization/available-locales @(make {"sv" "" "en" ""}))))
  (is (= ["en_US"] (localization/available-locales @(make {"en-us" ""})))))

(deftest listener-test
  (let [localization (make {"en" "hello = Hello, {name}."})
        label (Label.)]
    (localization/localize! label localization (localization/message "hello" {"name" "User"}))
    (is (= "Hello, User." (.getText label)))
    (localization/set-bundle! localization :test (bundle {"en" "hello = Hi, {name}!"}))
    (localization/await-for-updates localization)
    (is (= "Hi, User!" (.getText label)))))

;; Verifies that listener registration, removal, and unchanged locale or bundle
;; updates do not notify watches and trigger unrelated UI refreshes.
(deftest watches-ignore-unchanged-state-test
  (let [test-bundle (bundle {"en" "hello = Hello!"})
        localization (localization/make (test-util/make-test-prefs) :test test-bundle #(throw %))
        initial-state @localization
        notifications (atom [])
        label (Label.)]
    (add-watch localization ::watch
               (fn [key _reference old-state new-state]
                 (swap! notifications conj [key old-state new-state])))

    (localization/localize! label localization (localization/message "hello"))
    (is (= "Hello!" (.getText label)))
    (localization/await-for-updates localization)

    (localization/localize! label localization "Fixed text")
    (is (= "Fixed text" (.getText label)))
    (localization/unlocalize! label localization)
    (localization/set-locale! localization (localization/current-locale initial-state))
    (localization/set-bundle! localization :test test-bundle)
    (localization/await-for-updates localization)

    (is (identical? initial-state @localization))
    (is (= [] @notifications))))

;; Verifies that actual language and bundle changes notify watches exactly once,
;; still refresh localized labels, and respect watch replacement and removal.
(deftest watches-notify-localization-changes-test
  (let [localization (make {"en" "hello = Hello!"
                            "sv" "hello = Hej!"})
        initial-state @localization
        notifications (atom [])
        label (Label.)]
    (localization/localize! label localization (localization/message "hello"))
    (localization/await-for-updates localization)
    (add-watch localization ::watch
               (fn [key _reference old-state new-state]
                 (swap! notifications conj [key old-state new-state])))

    (localization/set-locale! localization "sv")
    (localization/await-for-updates localization)
    (let [locale-state @localization]
      (is (= [[::watch initial-state locale-state]] @notifications))
      (is (= "Hej!" (.getText label)))

      (add-watch localization ::watch
                 (fn [key _reference old-state new-state]
                   (swap! notifications conj [:replacement key old-state new-state])))
      (localization/set-bundle! localization :test (bundle {"en" "hello = Goodbye!"
                                                            "sv" "hello = Hej då!"}))
      (localization/await-for-updates localization)
      (let [expected-notifications [[::watch initial-state locale-state]
                                    [:replacement ::watch locale-state @localization]]]
        (is (= expected-notifications @notifications))
        (is (= "Hej då!" (.getText label)))

        (remove-watch localization ::watch)
        (localization/set-locale! localization "en")
        (localization/await-for-updates localization)
        (is (= expected-notifications @notifications))
        (is (= "Goodbye!" (.getText label)))))))

(deftest listener-weak-reference-test
  (let [localization (make {"en" "hello = Hello, {name}."})
        update-count (atom 0)
        obj (reify localization/Localizable
              (apply-localization [_ _]
                (swap! update-count inc)))]
    (localization/localize! obj localization (localization/message "hello" {"name" "User"}))
    (is (= 1 @update-count))
    (localization/set-bundle! localization :test (bundle {"en" "hello = Hi, {name}!"}))
    (localization/await-for-updates localization)
    ;; just need to keep the reference until after the update
    (some? obj)
    (is (= 2 @update-count))
    (System/gc)
    (localization/set-bundle! localization :test (bundle {"en" "hello = Hey there, {name}!"}))
    (localization/await-for-updates localization)
    ;; no more updates: obj is garbage-collected
    (is (= 2 @update-count))))

(deftest ellipsis-test
  (let [errors (util/group-into
                 {} []
                 :path identity
                 (coll/into-> (fs/class-path-walker java/class-loader "localization") :eduction
                   (filter #(.endsWith (str %) ".editor_localization"))
                   (mapcat (fn [path]
                             (e/map #(-> {:path (str (.getFileName (path/of path))) :key (key %) :string (val %)})
                                    (java-properties/parse (io/reader path)))))
                   (filter #(string/includes? (:string %) "..."))))]
    (is (empty? errors)
        (coll/join-to-string
          "\n"
          (coll/into-> errors :eduction
            (map (fn [path+error]
                   (str "Triple dots (...) instead of ellipsis (…) in " (key path+error) ":\n"
                        (coll/join-to-string "\n" (e/map #(str (:key %) " = " (:string %)) (val path+error)))))))))))

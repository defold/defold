(ns integration.font-test
  (:require [clojure.test :refer :all]
            [clojure.string :as s]
            [dynamo.graph :as g]
            [integration.test-util :as test-util]
            [editor.workspace :as workspace]
            [editor.font :as font]
            [editor.defold-project :as project]))

(defn- prop [node-id label]
  (get-in (g/node-value node-id :_properties) [:properties label :value]))

(defn- prop! [node-id label val]
  (g/transact (g/set-property node-id label val)))

(deftest load-material-render-data
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")
          scene (g/node-value node-id :scene)]
      (is (not (nil? scene))))))

(deftest text-measure
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")
          font-map (g/node-value node-id :font-map)]
      (let [[w h] (font/measure font-map "test")]
        (is (> w 0))
        (is (> h 0))
        (let [[w' h'] (font/measure font-map "test\ntest")]
          (is (= w' w))
          (is (> h' h))
          (let [[w'' h''] (font/measure font-map "test test test" true w 0 1)]
            (is (= w'' w'))
            (is (> h'' h')))
          (let [[w'' h''] (font/measure font-map "test test test" true w 0.1 1.1)]
            (is (> w'' w'))
            (is (> h'' h'))
            (let [[w''' h'''] (font/measure font-map "test\u200Btest test" true w 0.1 1.1)]
              (is (<= w''' w''))
              (is (= h''' h'')))))))))

(deftest preview-text
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")
          font-map (g/node-value node-id :font-map)
          pre-text (g/node-value node-id :preview-text)
          no-break (s/replace pre-text " " "")
          [w h] (font/measure font-map pre-text true (:cache-width font-map) 0 1)
          [ew eh] (font/measure font-map no-break true (:cache-width font-map) 0 1)]
      (is (.contains pre-text " "))
      (is (not (.contains no-break " ")))
      (is (< w ew))
      (is (< eh h)))))

(deftest validation
  (test-util/with-loaded-project
    (let [node-id   (test-util/resource-node project "/fonts/score.font")]
      (is (nil? (test-util/prop-error node-id :font)))
      (is (nil? (test-util/prop-error node-id :material)))
      (test-util/with-prop [node-id :font nil]
        (is (g/error-fatal? (test-util/prop-error node-id :font))))
      (test-util/with-prop [node-id :font (workspace/resolve-workspace-resource workspace "/not_found.ttf")]
        (is (g/error-fatal? (test-util/prop-error node-id :font))))
      (test-util/with-prop [node-id :material nil]
        (is (g/error-fatal? (test-util/prop-error node-id :material))))
      (test-util/with-prop [node-id :material (workspace/resolve-workspace-resource workspace "/not_found.material")]
        (is (g/error-fatal? (test-util/prop-error node-id :material))))
      (doseq [p [:size :alpha :outline-alpha :outline-width :shadow-alpha :shadow-blur :cache-width :cache-height]]
        (test-util/with-prop [node-id p -1]
          (is (g/error-fatal? (test-util/prop-error node-id p))))))))

(defn pb-property [node-id property]
  (get-in (g/node-value node-id :pb-msg) [property]))

(deftest antialias
  (test-util/with-loaded-project
    (let [score    (test-util/resource-node project "/fonts/score.font")
          score-not-antialias (test-util/resource-node project "/fonts/score_not_antialias.font")
          score-no-antialias  (test-util/resource-node project "/fonts/score_no_antialias.font")]

      (is (= (g/node-value score :antialiased) true))
      (is (= (g/node-value score :antialias) 1))
      (is (= (pb-property score :antialias) 1))

      (g/set-property! score :antialiased false)
      (is (= (g/node-value score :antialias) 0))
      (is (= (pb-property score :antialias) 0))

      (is (= (g/node-value score-not-antialias :antialiased) false))
      (is (= (g/node-value score-not-antialias :antialias) 0))
      (is (= (pb-property score-not-antialias :antialias) 0))

      (g/set-property! score-not-antialias :antialiased true)
      (is (= (g/node-value score-not-antialias :antialias) 1))
      (is (= (pb-property score-not-antialias :antialias) 1))

      (is (= (g/node-value score-no-antialias :antialiased) true)) ; font_ddf defaults antialias to 1 = true
      (is (= (g/node-value score-no-antialias :antialias) 1))
      (is (= (pb-property score-no-antialias :antialias) 1))

      (g/set-property! score-no-antialias :antialiased false)
      (is (= (g/node-value score-no-antialias :antialias) 0))
      (is (= (pb-property score-no-antialias :antialias) 0))

      (g/set-property! score-no-antialias :antialiased true)
      (is (= (g/node-value score-no-antialias :antialias) 1))
      (is (= (pb-property score-no-antialias :antialias) 1))

      (g/set-property! score-no-antialias :antialias nil)
      (is (= (g/node-value score-no-antialias :antialias) nil))
      (is (= (pb-property score-no-antialias :antialias) nil))

      (g/set-property! score-no-antialias :antialias 1)
      (is (= (g/node-value score-no-antialias :antialiased) true))
      (g/set-property! score-no-antialias :antialias 0)
      (is (= (g/node-value score-no-antialias :antialiased) false))
      (g/set-property! score-no-antialias :antialias nil)
      (is (= (g/node-value score-no-antialias :antialiased) nil)))))

(deftest font-scene
  (test-util/with-loaded-project
    (let [node-id (project/get-resource-node project "/fonts/logo.font")]
      (test-util/test-uses-assigned-material workspace project node-id
                                             :material
                                             [:renderable :user-data :shader]
                                             [:renderable :user-data :texture]))))

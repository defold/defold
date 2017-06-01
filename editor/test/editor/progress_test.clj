(ns editor.progress-test
  (:refer-clojure :exclude [mapv])
  (:require [clojure.test :refer :all]
            [editor.progress :refer :all]))

(deftest make-test
  (is (= {:message "mess" :size 10 :pos 0}
         (make "mess" 10)))
  (is (= {:message "mess" :size 10 :pos 5}
         (make "mess" 10 5))))

(deftest message-test
  (is (= {:message "mess2" :size 1 :pos 0}
         (message (make "mess") "mess2"))))

(deftest advance-test
  (is (= {:message "mess" :size 1 :pos 1}
         (advance (make "mess"))))

  (is (= {:message "mess2" :size 2 :pos 2}
         (advance (make "mess" 2) 2 "mess2")))

  (testing "advancing beyond size"
    (is (= {:message "mess2" :size 2 :pos 2}
           (advance (make "mess" 2) 3 "mess2")))))

(deftest precentage-test
  (is (= 0 (percentage (make "mess"))))
  (is (= 1 (percentage (advance (make "mess")))))
  (is (= 1/2 (percentage (advance (make "mess" 2)))))
  (is (= 0 (percentage nil)))

  (testing "subjobs contribution to percentage are weighted"
    (is (= 0 (percentage [(make "mess") (make "mess2")])))
    (is (= 1/2 (percentage [(advance (make "mess")) (make "mess2" 2)])))
    (is (= 1/5 (percentage [(advance (make "mess")) (make "mess2" 5)])))
    (is (= 1/4 (percentage [(advance (make "mess" 2)) (make "mess2" 2)])))
    (is (= 1/2 (percentage [(advance (make "mess" 2) 2) (make "mess2" 2)])))
    (is (= 1/3 (percentage [(advance (make "mess")) (make "mess2") (make "mess3" 3)])))
    (is (= 1/6 (percentage [(advance (make "mess")) (make "mess2" 2) (make "mess3" 3)]))))

  (testing "missing size of parent of subjobs leading strange percentage"
    (is (= 1 (percentage [(advance (make "mess")) (make "mess2")])))))

(deftest description-test
  (is (= "mess" (description (make "mess"))))
  (is (= "mess\nmess2" (description [(make "mess") (make "mess2")])))
  (is (= "" (description nil))))

(deftest nest-render-progress-test
  (let [res      (atom nil)
        render-p #(reset! res %)
        j1       (make "mess")
        j2       (make "mess2")]
    ((nest-render-progress render-p j2) j1)
    (is (= [j1 j2] @res))))

(deftest progress-mapv-test
  (let [render-res (atom [])
        render-f   #(swap! render-res conj %)
        message-f  #(str "m" %)]

    (is (= (map inc (range 3))
           (progress-mapv (fn [d _ ] (inc d)) (range 3) render-f message-f)))
    (is (= [(make "m0" 3 0)
            (make "m1" 3 1)
            (make "m2" 3 2)]
           @render-res))

    (reset! render-res [])

    (testing "empty col"
      (is (= [] (progress-mapv inc [] render-f)))
      (is (= [] @render-res)))

    (testing "nil"
      (is (= [] (progress-mapv inc nil render-f)))
      (is (= [] @render-res)))))

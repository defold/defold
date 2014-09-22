(ns dynamo.camera-test
  (:require [dynamo.camera :refer :all]
            [dynamo.ui :refer :all]
            [clojure.test :refer :all])
  (:import [org.eclipse.swt SWT]
           [org.eclipse.swt.widgets Event Composite Shell]
           [org.eclipse.swt.events  MouseEvent]))

(defn- mouse-event
  [modifiers button]
  (let [evt (promise)]
    (swt-safe
      (let [shell (Shell.)
           event (Event.)]
       (set! (. event button)    button)
       (set! (. event type)      SWT/MouseDown)
       (set! (. event stateMask) (reduce bit-and SWT/MODIFIER_MASK modifiers))
       (set! (. event widget)    shell)
       (deliver evt (MouseEvent. event))))
    @evt))

(deftest mouse-button-interpretation
  (testing "one-button-movements"
           (are [move button mods] (= move (camera-movement :one-button button mods))
                :track  1 (bit-or SWT/ALT SWT/CTRL)
                :rotate 1 SWT/ALT
                :dolly  1 SWT/CTRL
                :idle   1 0))
  (testing "three-button-movements"
           (are [move button mods] (= move (camera-movement :three-button button mods))
                :track  2 SWT/ALT
                :rotate 1 SWT/ALT
                :dolly  3 SWT/ALT
                :idle   1 0
                :idle   3 SWT/CTRL
                :idle   3 0
                :idle   2 SWT/CTRL)))



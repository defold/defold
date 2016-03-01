(ns editor.colors)

(set! *warn-on-reflection* true)

(defn- hex-color->color [str]
  (let [conv (fn [s] (/ (Integer/parseInt s 16) 255.0))]
    [(conv (subs str 1 3))
     (conv (subs str 3 5))
     (conv (subs str 5 7))
     1.0]))

(def defold-light-blue (hex-color->color "#60a8ff"))
(def defold-blue (hex-color->color "#2268e6"))
(def defold-white (hex-color->color "#f0f2f6"))
(def defold-white-light (hex-color->color "#f8f8fb"))
(def defold-red (hex-color->color "#ff4646"))
(def defold-yellow (hex-color->color "#fbce2f"))
(def defold-turquoise (hex-color->color "#00e6e1"))
(def defold-green (hex-color->color "#00df64"))
(def defold-orange (hex-color->color "#fd6623"))
(def selected-blue (hex-color->color "#264a8b"))
(def bright-black (hex-color->color "#292c30"))
(def bright-black-light (hex-color->color "#2e3236"))
(def mid-black (hex-color->color "#25282b"))
(def mid-black-light (hex-color->color "#292d31"))
(def dark-black (hex-color->color "#222426"))
(def dark-black-light (hex-color->color "#26282a"))
(def dark-grey (hex-color->color "#34373b"))
(def dark-grey-light (hex-color->color "#3b3e43"))
(def mid-grey (hex-color->color "#494b4e"))
(def mid-grey-light (hex-color->color "#54565a"))
(def bright-grey (hex-color->color "#8f9296"))
(def bright-grey-light (hex-color->color "#a8abaf"))

(defn alpha [c a]
  (assoc c 3 a))

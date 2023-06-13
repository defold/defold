;; Copyright 2020-2023 The Defold Foundation
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

(ns util.num
  (:import [clojure.lang RT]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmacro normalized->byte [num]
  `(RT/uncheckedByteCast (Math/floor (* (double ~num) 127.5))))

(defmacro normalized->ubyte [num]
  `(RT/uncheckedByteCast (Math/round (* (double ~num) 255.0))))

(defmacro normalized->short [num]
  `(RT/uncheckedShortCast (Math/floor (* (double ~num) 32767.5))))

(defmacro normalized->ushort [num]
  `(RT/uncheckedShortCast (Math/round (* (double ~num) 65535.0))))

(defmacro normalized->int [num]
  `(RT/uncheckedIntCast (Math/floor (* (double ~num) 2147483647.5))))

(defmacro normalized->uint [num]
  `(RT/uncheckedIntCast (Math/round (* (double ~num) 4294967295.0))))

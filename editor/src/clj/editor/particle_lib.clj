(ns editor.particle-lib
  (:import [com.defold.libs ParticleLibrary]
           [com.sun.jna Pointer]))

(defn create-context [max-emitter-count max-particle-count]
  (ParticleLibrary/Particle_CreateContext max-emitter-count max-particle-count))

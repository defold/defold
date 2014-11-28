(ns dynamo.particle-lib
  (:import [com.dynamo.cr.parted ParticleLibrary]
           [com.sun.jna Pointer]))

(defn create-context [max-emitter-count max-particle-count]
  (ParticleLibrary/Particle_CreateContext max-emitter-count max-particle-count))

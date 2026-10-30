// A merged layout covers all resources used by the three stages.
#define MERGED "DescriptorTable(CBV(b0, space=1)), DescriptorTable(SRV(t0, numDescriptors=2, space=2)), DescriptorTable(Sampler(s0, space=2)), DescriptorTable(UAV(u0, space=3))"
// Deliberately omits the resource ranges needed by the shaders.
#define INCOMPATIBLE "DescriptorTable(CBV(b7))"

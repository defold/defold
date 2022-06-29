
// Sample properties when viewed as an array of floats
const g_nbFloatsPerSample = 14;
const g_sampleOffsetFloats_NameOffset = 0;
const g_sampleOffsetFloats_NameLength = 1;
const g_sampleOffsetFloats_Start = 3;
const g_sampleOffsetFloats_Length = 5;
const g_sampleOffsetFloats_Self = 7;
const g_sampleOffsetFloats_GpuToCpu = 9;
const g_sampleOffsetFloats_Calls = 11;
const g_sampleOffsetFloats_Recurse = 12;

// Sample properties when viewed as bytes
const g_nbBytesPerSample = g_nbFloatsPerSample * 4;
const g_sampleOffsetBytes_NameOffset = g_sampleOffsetFloats_NameOffset * 4;
const g_sampleOffsetBytes_NameLength = g_sampleOffsetFloats_NameLength * 4;
const g_sampleOffsetBytes_Colour = 8;
const g_sampleOffsetBytes_Depth = 11;
const g_sampleOffsetBytes_Start = g_sampleOffsetFloats_Start * 4;
const g_sampleOffsetBytes_Length = g_sampleOffsetFloats_Length * 4;
const g_sampleOffsetBytes_Self = g_sampleOffsetFloats_Self * 4;
const g_sampleOffsetBytes_GpuToCpu = g_sampleOffsetFloats_GpuToCpu * 4;
const g_sampleOffsetBytes_Calls = g_sampleOffsetFloats_Calls * 4;
const g_sampleOffsetBytes_Recurse = g_sampleOffsetFloats_Recurse * 4;

// Snapshot properties
const g_nbFloatsPerSnapshot = 10;
const g_nbBytesPerSnapshot = g_nbFloatsPerSnapshot * 4;
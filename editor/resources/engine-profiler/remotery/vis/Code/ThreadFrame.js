

class ThreadFrame
{
	constructor(message)
	{
		// Persist the required message data
		this.NbSamples = message.nb_samples;
		this.sampleDataView = message.sampleDataView;
		this.sampleFloats = message.sampleFloats;
		this.PartialTree = message.partial_tree > 0 ? true : false;

		// Point to the first/last samples
		const first_sample_start_us = this.sampleFloats[g_sampleOffsetFloats_Start] * 1000.0;
		const last_sample_offset = (this.NbSamples - 1) * g_nbFloatsPerSample;
		const last_sample_start_us = this.sampleFloats[last_sample_offset + g_sampleOffsetFloats_Start] * 1000.0;
		const last_sample_length_us = this.sampleFloats[last_sample_offset + g_sampleOffsetFloats_Length] * 1000.0;

		// Calculate the frame start/end times
		this.StartTime_us = first_sample_start_us;
		this.EndTime_us = last_sample_start_us + last_sample_length_us;
		this.Length_us = this.EndTime_us - this.StartTime_us;
	}
}

class PropertySnapshotFrame
{
	constructor(message)
	{
		this.nbSnapshots = message.nbSnapshots;
		this.snapshots = message.snapshots;
		this.snapshotFloats = message.snapshotFloats;
	}
}
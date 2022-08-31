
class PixelTimeRange
{
	constructor(start_us, span_us, span_px)
	{
		this.Span_px = span_px;
		this.Set(start_us, span_us);
	}

	Set(start_us, span_us)
	{
		this.Start_us = start_us;
		this.Span_us = span_us;
		this.End_us = this.Start_us + span_us;
		this.usPerPixel = this.Span_px / this.Span_us;
	}

	SetStart(start_us)
	{
		this.Start_us = start_us;
		this.End_us = start_us + this.Span_us;
	}

	SetEnd(end_us)
	{
		this.End_us = end_us;
		this.Start_us = end_us - this.Span_us;
	}

	SetPixelSpan(span_px)
	{
		this.Span_px = span_px;
		this.usPerPixel = this.Span_px / this.Span_us;
	}

	PixelOffset(time_us)
	{
		return Math.floor((time_us - this.Start_us) * this.usPerPixel);
	}

	PixelSize(time_us)
	{
		return Math.floor(time_us * this.usPerPixel);
	}

	TimeAtPosition(position)
	{
		return this.Start_us + position / this.usPerPixel;
	}

	Clone()
	{
		return new PixelTimeRange(this.Start_us, this.Span_us, this.Span_px);
	}

	SetAsUniform(gl, program)
	{
		glSetUniform(gl, program, "inTimeRange.msStart", this.Start_us / 1000.0);
		glSetUniform(gl, program, "inTimeRange.msPerPixel", this.usPerPixel * 1000.0);
	}
}

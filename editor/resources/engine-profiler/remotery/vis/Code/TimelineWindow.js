
// TODO(don): Separate all knowledge of threads from this timeline

TimelineWindow = (function()
{
	var BORDER = 10;

	function TimelineWindow(wm, name, settings, check_handler)
	{
		this.Settings = settings;

		// Create timeline window
		this.Window = wm.AddWindow("Timeline", 10, 20, 100, 100);
		this.Window.SetTitle(name);
		this.Window.ShowNoAnim();

		this.timelineMarkers = new TimelineMarkers(this);

		// DO THESE need to be containers... can they just be divs?
		// divs need a retrieval function
		this.TimelineLabelScrollClipper = this.Window.AddControlNew(new WM.Container(10, 10, 10, 10));
		DOM.Node.AddClass(this.TimelineLabelScrollClipper.Node, "TimelineLabelScrollClipper");
		this.TimelineLabels = this.TimelineLabelScrollClipper.AddControlNew(new WM.Container(0, 0, 10, 10));
		DOM.Node.AddClass(this.TimelineLabels.Node, "TimelineLabels");

		// Ordered list of thread rows on the timeline
		this.ThreadRows = [ ];

		// Create timeline container
		this.TimelineContainer = this.Window.AddControlNew(new WM.Container(10, 10, 800, 160));
		DOM.Node.AddClass(this.TimelineContainer.Node, "TimelineContainer");

		var mouse_wheel_event = (/Firefox/i.test(navigator.userAgent)) ? "DOMMouseScroll" : "mousewheel";
		DOM.Event.AddHandler(this.TimelineContainer.Node, mouse_wheel_event, Bind(OnMouseScroll, this));

		// Setup timeline manipulation
		this.MouseDown = false;
		this.LastMouseState = null;
		this.TimelineMoved = false;
		DOM.Event.AddHandler(this.TimelineContainer.Node, "mousedown", Bind(OnMouseDown, this));
		DOM.Event.AddHandler(this.TimelineContainer.Node, "mouseup", Bind(OnMouseUp, this));
		DOM.Event.AddHandler(this.TimelineContainer.Node, "mousemove", Bind(OnMouseMove, this));
		DOM.Event.AddHandler(this.TimelineContainer.Node, "mouseleave", Bind(OnMouseLeave, this));

		// Create a canvas for timeline 2D rendering
		// TODO(don): Port this to shaders
		this.drawCanvas = document.createElement("canvas");
		this.drawCanvas.width = this.TimelineContainer.Node.clientWidth;
		this.drawCanvas.height = this.TimelineContainer.Node.clientHeight;
		this.TimelineContainer.Node.appendChild(this.drawCanvas);
		this.drawContext = this.drawCanvas.getContext("2d");

		// Create a canvas for timeline 3D accelerated rendering
		this.glCanvas = document.createElement("canvas");
		this.glCanvas.width = this.TimelineContainer.Node.clientWidth;
		this.glCanvas.height = this.TimelineContainer.Node.clientHeight;
		this.TimelineContainer.Node.appendChild(this.glCanvas);

		// OVERLAY - add to CSS
		this.glCanvas.style.position = "absolute";
		this.glCanvas.style.top = 0;
		this.glCanvas.style.left = 0;

		// For now a gl context per timeline
		let gl = this.glCanvas.getContext("webgl2");
		this.gl = gl;

		const vshader = glCompileShader(gl, gl.VERTEX_SHADER, "TimelineVShader", TimelineVShader);
		const fshader = glCompileShader(gl, gl.FRAGMENT_SHADER, "TimelineFShader", TimelineFShader);
		this.Program = glCreateProgram(gl, vshader, fshader);

		this.font = new glFont(gl);
		this.textBuffer = new glTextBuffer(gl, this.font);

		this.Window.SetOnResize(Bind(OnUserResize, this));

		this.Clear();

		this.OnHoverHandler = null;
		this.OnSelectedHandler = null;
		this.OnMovedHandler = null;
		this.CheckHandler = check_handler;

		this.yScrollOffset = 0;

		this.HoverSampleInfo = null;
	}


	TimelineWindow.prototype.Clear = function()
	{
		// Clear out labels
		this.TimelineLabels.ClearControls();

		this.ThreadRows = [ ];
		this.TimeRange = new PixelTimeRange(0, 200 * 1000, this.TimelineContainer.Node.clientWidth);
	}


	TimelineWindow.prototype.SetOnHover = function(handler)
	{
		this.OnHoverHandler = handler;
	}


	TimelineWindow.prototype.SetOnSelected = function(handler)
	{
		this.OnSelectedHandler = handler;
	}


	TimelineWindow.prototype.SetOnMoved = function(handler)
	{
		this.OnMovedHandler = handler;
	}


	TimelineWindow.prototype.WindowResized = function(x, width, top_window)
	{
		// Resize window
		var top = top_window.Position[1] + top_window.Size[1] + 10;
		this.Window.SetPosition(x, top);
		this.Window.SetSize(width - 2 * 10, 260);

		ResizeInternals(this);
	}


	TimelineWindow.prototype.OnSamples = function(thread_name, frame_history)
	{
		// Shift the timeline to the last entry on this thread
		// As multiple threads come through here with different end frames, only do this for the latest
		var last_frame = frame_history[frame_history.length - 1];
		if (last_frame.EndTime_us > this.TimeRange.End_us)
			this.TimeRange.SetEnd(last_frame.EndTime_us);

		// Search for the index of this thread
		var thread_index = -1;
		for (var i in this.ThreadRows)
		{
			if (this.ThreadRows[i].Name == thread_name)
			{
				thread_index = i;
				break;
			}
		}

		// If this thread has not been seen before, add a new row to the list and re-sort
		if (thread_index == -1)
		{
			var row = new TimelineRow(this.gl, thread_name, this, frame_history, this.CheckHandler);
			this.ThreadRows.push(row);
		}
	}


	TimelineWindow.prototype.DrawBackground = function()
	{
		// TODO(don): Port all this lot to shader, maybe... it's not performance sensitive

		this.drawContext.clearRect(0, 0, this.drawCanvas.width, this.drawCanvas.height);

		// Draw thread row backgrounds
		for (let thread_row of this.ThreadRows)
		{
			thread_row.DrawBackground(this.HoverSampleInfo, this.yScrollOffset);
		}

		this.timelineMarkers.Draw(this.TimeRange);
	}


	TimelineWindow.prototype.DrawAllRows = function()
	{
		let gl = this.gl;

		gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);

        gl.useProgram(this.Program);

        // Set viewport parameters
        glSetUniform(gl, this.Program, "inViewport.width", gl.canvas.width);
        glSetUniform(gl, this.Program, "inViewport.height", gl.canvas.height);

		// Set time range parameters
		const time_range = this.TimeRange;
        time_range.SetAsUniform(gl, this.Program);

        // Set text rendering resources
		// Note it might not be loaded yet so we need the null check
		if (this.font.atlasTexture != null)
		{
			glSetUniform(gl, this.Program, "inFontAtlasTextre", this.font.atlasTexture, 0);
        	this.textBuffer.SetAsUniform(gl, this.Program, "inTextBuffer", 1);
		}

		const draw_text = this.Settings.IsPaused;
		for (let i in this.ThreadRows)
		{
			var thread_row = this.ThreadRows[i];
			thread_row.SetVisibleFrames(time_range);
			thread_row.Draw(gl, draw_text, this.yScrollOffset);
		}

		// Render last so that each thread row uses any new time ranges
		this.DrawBackground();
	}


	function OnUserResize(self, evt)
	{
		ResizeInternals(self);
	}

	function ResizeInternals(self)
	{
		// .TimelineRowLabel
		// .TimelineRowExpand
		// .TimelineRowExpand
		// .TimelineRowCheck
		// Window padding
		let offset_x = 145+19+19+19+10;

		let MarkersHeight = 18;

		var parent_size = self.Window.Size;

		self.timelineMarkers.Resize(BORDER + offset_x, 10, parent_size[0] - 2* BORDER - offset_x, MarkersHeight);

		// Resize controls
		self.TimelineContainer.SetPosition(BORDER + offset_x, 10 + MarkersHeight);
		self.TimelineContainer.SetSize(parent_size[0] - 2 * BORDER - offset_x, parent_size[1] - MarkersHeight - 40);

		self.TimelineLabelScrollClipper.SetPosition(10, 10 + MarkersHeight);
		self.TimelineLabelScrollClipper.SetSize(offset_x, parent_size[1] - MarkersHeight - 40);
		self.TimelineLabels.SetSize(offset_x, parent_size[1] - MarkersHeight - 40);

		// Match canvas size to container
		const width = self.TimelineContainer.Node.clientWidth;
		const height = self.TimelineContainer.Node.clientHeight;
		self.drawCanvas.width = width;
		self.drawCanvas.height = height;
		self.glCanvas.width = width;
		self.glCanvas.height = height;

		// Adjust time range to new width
		self.TimeRange.SetPixelSpan(width);
		self.DrawAllRows();
	}


	function OnMouseScroll(self, evt)
	{
		let mouse_state = new Mouse.State(evt);
		let scale = 1.11;
		if (mouse_state.WheelDelta > 0)
			scale = 1 / scale;

		// What time is the mouse hovering over?
		let mouse_pos = self.TimelineMousePosition(mouse_state);
		let time_us = self.TimeRange.TimeAtPosition(mouse_pos[0]);

		// Calculate start time relative to the mouse hover position
		var time_start_us = self.TimeRange.Start_us - time_us;

		// Scale and offset back to the hover time
		self.TimeRange.Set(time_start_us * scale + time_us, self.TimeRange.Span_us * scale);
		self.DrawAllRows();

		if (self.OnMovedHandler)
		{
			self.OnMovedHandler(self);
		}

		// Prevent vertical scrolling on mouse-wheel
		DOM.Event.StopDefaultAction(evt);
	}


	TimelineWindow.prototype.SetTimeRange = function(start_us, span_us)
	{
		this.TimeRange.Set(start_us, span_us);
	}


	TimelineWindow.prototype.DisplayHeight = function()
	{
		// Sum height of each thread row
		let height = 0;
		for (thread_row of this.ThreadRows)
		{
			height += thread_row.DisplayHeight();
		}

		return height;
	}


	TimelineWindow.prototype.ScrollVertically = function(y_scroll)
	{
		// Calculate the minimum negative value the position of the labels can be to account for scrolling to the bottom
		// of the label/depth list
		let display_height = this.DisplayHeight();
		let container_height = this.TimelineLabelScrollClipper.Node.clientHeight;
		let minimum_y = Math.min(container_height - display_height, 0.0);

		// Resize the label container to match the display height
		this.TimelineLabels.Node.style.height = Math.max(display_height, container_height);

		// Increment the y-scroll using just-calculated limits
		let old_y_scroll_offset = this.yScrollOffset;
		this.yScrollOffset = Math.min(Math.max(this.yScrollOffset + y_scroll, minimum_y), 0);

		// Calculate how much the labels should actually scroll after limiting and apply
		let y_scroll_px = this.yScrollOffset - old_y_scroll_offset;
		this.TimelineLabels.Node.style.top = this.TimelineLabels.Node.offsetTop + y_scroll_px;
	}


	TimelineWindow.prototype.TimelineMousePosition = function(mouse_state)
	{
		// Position of the mouse relative to the timeline container
		let node_offset = DOM.Node.GetPosition(this.TimelineContainer.Node);
		let mouse_x = mouse_state.Position[0] - node_offset[0];
		let mouse_y = mouse_state.Position[1] - node_offset[1];

		// Offset by the amount of scroll
		mouse_y -= this.yScrollOffset;

		return [ mouse_x, mouse_y ];
	}


	TimelineWindow.prototype.GetHoverThreadRow = function(mouse_pos)
	{
		// Search for the thread row the mouse intersects
		let height = 0;
		for (let thread_row of this.ThreadRows)
		{
			let row_height = thread_row.DisplayHeight();
			if (mouse_pos[1] >= height && mouse_pos[1] < height + row_height)
			{
				// Mouse y relative to row start
				mouse_pos[1] -= height;
				return thread_row;
			}
			height += row_height;
		}

		return null;
	}


	function OnMouseDown(self, evt)
	{
		// Only manipulate the timeline when paused
		if (!self.Settings.IsPaused)
			return;

		self.MouseDown = true;
		self.LastMouseState = new Mouse.State(evt);
		self.TimelineMoved = false;
		DOM.Event.StopDefaultAction(evt);
	}


	function OnMouseUp(self, evt)
	{
		// Only manipulate the timeline when paused
		if (!self.Settings.IsPaused)
			return;

		var mouse_state = new Mouse.State(evt);

		self.MouseDown = false;

		if (!self.TimelineMoved)
		{
			// Are we hovering over a thread row?
			let mouse_pos = self.TimelineMousePosition(mouse_state);
			let hover_thread_row = self.GetHoverThreadRow(mouse_pos);
			if (hover_thread_row != null)
			{
				// Are we hovering over a sample?
				let time_us = self.TimeRange.TimeAtPosition(mouse_pos[0]);
				let sample_info = hover_thread_row.GetSampleAtPosition(time_us, mouse_pos[1]);
				if (sample_info != null)
				{
					// Redraw with new select sample
					hover_thread_row.SetSelectSample(sample_info);
					self.DrawBackground();

					// Call any selection handlers
					if (self.OnSelectedHandler)
					{
						self.OnSelectedHandler(hover_thread_row.Name, sample_info);
					}
				}
			}
		}
	}


	function OnMouseMove(self, evt)
	{
		// Only manipulate the timeline when paused
		if (!self.Settings.IsPaused)
			return;

		var mouse_state = new Mouse.State(evt);

		if (self.MouseDown)
		{
			let movement = false;

			// Shift the visible time range with mouse movement
			let time_offset_us = (mouse_state.Position[0] - self.LastMouseState.Position[0]) / self.TimeRange.usPerPixel;
			if (time_offset_us != 0)
			{
				self.TimeRange.SetStart(self.TimeRange.Start_us - time_offset_us);
				movement = true;
			}

			// Control vertical movement
			let y_offset_px = mouse_state.Position[1] - self.LastMouseState.Position[1];
			if (y_offset_px != 0)
			{
				self.ScrollVertically(y_offset_px);
				movement = true;
			}

			// Redraw everything if there is movement
			if (movement)
			{
				self.DrawAllRows();
				self.TimelineMoved = true;

				if (self.OnMovedHandler)
				{
					self.OnMovedHandler(self);
				}
			}
		}

		else
		{
			// Are we hovering over a thread row?
			let mouse_pos = self.TimelineMousePosition(mouse_state);
			let hover_thread_row = self.GetHoverThreadRow(mouse_pos);
			if (hover_thread_row != null)
			{
				// Are we hovering over a sample?
				let time_us = self.TimeRange.TimeAtPosition(mouse_pos[0]);
				self.HoverSampleInfo = hover_thread_row.GetSampleAtPosition(time_us, mouse_pos[1]);

				// Tell listeners which sample we're hovering over
				if (self.OnHoverHandler != null)
				{
					self.OnHoverHandler(hover_thread_row.Name, self.HoverSampleInfo);
				}
			}
			else
			{
				self.HoverSampleInfo = null;
			}

			// Redraw to update highlights
			self.DrawBackground();
		}

		self.LastMouseState = mouse_state;
	}


	function OnMouseLeave(self, evt)
	{
		// Only manipulate the timeline when paused
		if (!self.Settings.IsPaused)
			return;
		
		// Cancel scrolling
		self.MouseDown = false;

		// Cancel hovering
		if (self.OnHoverHandler != null)
		{
			self.OnHoverHandler(null, null);
		}
	}


	return TimelineWindow;
})();


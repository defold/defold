
//
// TODO: Window resizing needs finer-grain control
// TODO: Take into account where user has moved the windows
// TODO: Controls need automatic resizing within their parent windows
//


Settings = (function()
{
    function Settings()
    {
        this.IsPaused = false;
        this.SyncTimelines = true;
    }

    return Settings;

})();


Remotery = (function()
{
    // crack the url and get the parameter we want
    var getUrlParameter = function getUrlParameter( search_param) 
    {
        var page_url = decodeURIComponent( window.location.search.substring(1) ),
                        url_vars = page_url.split('&'),
                        param_name,
                        i;

        for (i = 0; i < url_vars.length; i++) 
        {
            param_name = url_vars[i].split('=');

            if (param_name[0] === search_param) 
            {
                return param_name[1] === undefined ? true : param_name[1];
            }
        }
    };

    function Remotery()
    {
        this.WindowManager = new WM.WindowManager();
        this.Settings = new Settings();

        // "addr" param is ip:port and will override the local store version if passed in the URL
        var addr = getUrlParameter( "addr" );
        if ( addr != null )
            this.ConnectionAddress = "ws://" + addr + "/rmt";
        else
            this.ConnectionAddress = LocalStore.Get("App", "Global", "ConnectionAddress", "ws://127.0.0.1:17815/rmt");

        this.Server = new WebSocketConnection();
        this.Server.AddConnectHandler(Bind(OnConnect, this));
        this.Server.AddDisconnectHandler(Bind(OnDisconnect, this));

        // Create the console up front as everything reports to it
        this.Console = new Console(this.WindowManager, this.Server);

        // Create required windows
        this.TitleWindow = new TitleWindow(this.WindowManager, this.Settings, this.Server, this.ConnectionAddress);
        this.TitleWindow.SetConnectionAddressChanged(Bind(OnAddressChanged, this));
        this.SampleTimelineWindow = new TimelineWindow(this.WindowManager, "Sample Timeline", this.Settings, Bind(OnTimelineCheck, this));
        this.SampleTimelineWindow.SetOnHover(Bind(OnSampleHover, this));
        this.SampleTimelineWindow.SetOnSelected(Bind(OnSampleSelected, this));
        this.ProcessorTimelineWindow = new TimelineWindow(this.WindowManager, "Processor Timeline", this.Settings, null);

        this.SampleTimelineWindow.SetOnMoved(Bind(OnTimelineMoved, this));
        this.ProcessorTimelineWindow.SetOnMoved(Bind(OnTimelineMoved, this));

        this.TraceDrop = new TraceDrop(this);

        this.NbSampleWindows = 0;
        this.SampleWindows = { };
        this.FrameHistory = { };
        this.ProcessorFrameHistory = { };
        this.SelectedFrames = { };
        this.sampleNames = new NameMap(this.SampleTimelineWindow.textBuffer);
        this.threadNames = new NameMap(this.ProcessorTimelineWindow.textBuffer);

        this.Server.AddMessageHandler("SMPL", Bind(OnSamples, this));
        this.Server.AddMessageHandler("SSMP", Bind(OnSampleName, this));
        this.Server.AddMessageHandler("PRTH", Bind(OnProcessorThreads, this));
        this.Server.AddMessageHandler("THRN", Bind(OnThreadNames, this));

        // Kick-off the auto-connect loop
        AutoConnect(this);

        // Hook up resize event handler
        DOM.Event.AddHandler(window, "resize", Bind(OnResizeWindow, this));
        OnResizeWindow(this);

        // Hook up browser-native canvas refresh
        this.DisplayFrame = 0;
        this.LastKnownPause = this.Settings.IsPaused;
        var self = this;
        (function display_loop()
        {
            window.requestAnimationFrame(display_loop);
            DrawTimeline(self);
        })();
    }


    Remotery.prototype.Clear = function()
    {
        // Clear timelines
        this.SampleTimelineWindow.Clear();
        this.ProcessorTimelineWindow.Clear();

        // Close and clear all sample windows
        for (var i in this.SampleWindows)
        {
            var sample_window = this.SampleWindows[i];
            sample_window.Close();
        }
        this.NbSampleWindows = 0;
        this.SampleWindows = { };

        // Clear runtime data
        this.FrameHistory = { };
        this.ProcessorFrameHistory = { };
        this.SelectedFrames = { };
        this.sampleNames = new NameMap(this.SampleTimelineWindow.textBuffer);
        this.threadNames = new NameMap(this.ProcessorTimelineWindow.textBuffer);

        // Resize everything to fit new layout
        OnResizeWindow(this);
    }


    function AutoConnect(self)
    {
        // Only attempt to connect if there isn't already a connection or an attempt to connect
        if (!self.Server.Connected())
            self.Server.Connect(self.ConnectionAddress);

        // Always schedule another check
        window.setTimeout(Bind(AutoConnect, self), 2000);
    }


    function OnConnect(self)
    {
        // Connection address has been validated
        LocalStore.Set("App", "Global", "ConnectionAddress", self.ConnectionAddress);

        self.Clear();

        // Ensure the viewer is ready for realtime updates
        self.TitleWindow.Unpause();
    }

    function OnDisconnect(self)
    {
        // Pause so the user can inspect the trace
        self.TitleWindow.Pause();
    }


    function OnAddressChanged(self, node)
    {
        // Update and disconnect, relying on auto-connect to reconnect
        self.ConnectionAddress = node.value;
        self.Server.Disconnect();

        // Give input focus away
        return false;
    }


    function DrawTimeline(self)
    {
        // Has pause state changed?
        if (self.Settings.IsPaused != self.LastKnownPaused)
        {
            // When switching TO paused, draw one last frame to ensure the sample text gets drawn
            self.LastKnownPaused = self.Settings.IsPaused;
            self.SampleTimelineWindow.DrawAllRows();
            self.ProcessorTimelineWindow.DrawAllRows();
            return;
        }

        // Don't waste time drawing the timeline when paused
        if (self.Settings.IsPaused)
            return;

        // requestAnimationFrame can run up to 60hz which is way too much for drawing the timeline
        // Assume it's running at 60hz and skip frames to achieve 10hz instead
        // Doing this instead of using setTimeout because it's better for browser rendering (or; will be once WebGL is in use)
        // TODO: Expose as config variable because high refresh rate is great when using a separate viewiing machine
        if ((self.DisplayFrame % 10) == 0)
        {
            self.SampleTimelineWindow.DrawAllRows();
            self.ProcessorTimelineWindow.DrawAllRows();
        }

        self.DisplayFrame++;
    }


    function DecodeSample(self, data_view_reader)
    {
        var sample = {};

        // Get name hash and lookup name it map
        sample.name_hash = data_view_reader.GetUInt32();
        let [ name_exists, name ] = self.sampleNames.Get(sample.name_hash);
        sample.name = name;

        // If the name doesn't exist in the map yet, request it from the server
        if (!name_exists)
        {
            if (self.Server.Connected())
            {
                self.Server.Send("GSMP" + sample.name_hash);
            }
        }

        // Get the rest of the sample data
        sample.id = data_view_reader.GetUInt32();
        sample.colour = data_view_reader.GetStringOfLength(7);
        sample.us_start = data_view_reader.GetUInt64();
        sample.us_length = data_view_reader.GetUInt64();
        sample.us_self = data_view_reader.GetUInt64();
        sample.call_count = data_view_reader.GetUInt32();
        sample.recurse_depth = data_view_reader.GetUInt32();
        sample.stat_type = data_view_reader.GetStringOfLength(3);
        if (sample.stat_type == "I32")
            sample.stat_value = data_view_reader.GetInt32();
        else if (sample.stat_type == "F32")
            sample.stat_value = data_view_reader.GetFloat32();
        else {
            data_view_reader.GetUInt32();
            sample.stat_value = null;
        }
        sample.stat_desc_hash = data_view_reader.GetUInt32();
        let [ desc_name_exists, desc_name ] = self.sampleNames.Get(sample.stat_desc_hash);
        sample.stat_desc = desc_name;

        // If the name doesn't exist in the map yet, request it from the server
        if (!desc_name_exists)
        {
            if (self.Server.Connected())
            {
                self.Server.Send("GSMP" + sample.stat_desc_hash);
            }
        }

        // TODO(don): Get the profiler to pass these directly instead of hex colour
        const colour = parseInt(sample.colour.slice(1), 16);
        const r = (colour >> 16) & 255;
        const g = (colour >> 8) & 255;
        const b = colour & 255;
        sample.rgbColour = [ r, g, b ];

        // Calculate dependent properties
        sample.ms_length = (sample.us_length / 1000.0).toFixed(3);
        sample.ms_self = (sample.us_self / 1000.0).toFixed(3);

        // Recurse into children
        sample.children = [];
        DecodeSampleArray(self, data_view_reader, sample.children);

        return sample;
    }


    function DecodeSampleArray(self, data_view_reader, samples)
    {
        var nb_samples = data_view_reader.GetUInt32();
        for (var i = 0; i < nb_samples; i++)
        {
            var sample = DecodeSample(self, data_view_reader);
            samples.push(sample)
        }
    }


    function DecodeSamples(self, data_view_reader)
    {
        // Message-specific header
        let message = { };
        message.sample_tree_bytes = data_view_reader.GetUInt32();
        message.thread_name = data_view_reader.GetString();
        message.nb_samples = data_view_reader.GetUInt32();
        message.sample_digest = data_view_reader.GetUInt32();
        message.partial_tree = data_view_reader.GetUInt32();

        // Read samples
        message.samples = [];
        message.samples.push(DecodeSample(self, data_view_reader));

        return message;
    }


    function OnSamples(self, socket, data_view_reader)
    {
        // Discard any new samples while paused and connected
        // Otherwise this stops a paused Remotery from loading new samples from disk
        if (self.Settings.IsPaused && self.Server.Connected())
            return;

        // Binary decode incoming sample data
        var message = DecodeSamples(self, data_view_reader);
        var name = message.thread_name;

        // Add to frame history for this thread
        var thread_frame = new ThreadFrame(message);
        if (!(name in self.FrameHistory))
        {
            self.FrameHistory[name] = [ ];
        }
        var frame_history = self.FrameHistory[name];
        if (frame_history.length > 0 && frame_history[frame_history.length - 1].PartialTree)
        {
            // Always overwrite partial trees with new information
            frame_history[frame_history.length - 1] = thread_frame;
        }
        else
        {
            frame_history.push(thread_frame);
        }

        // Discard old frames to keep memory-use constant
        var max_nb_frames = 10000;
        var extra_frames = frame_history.length - max_nb_frames;
        if (extra_frames > 0)
            frame_history.splice(0, extra_frames);

        // Create sample windows on-demand
        if (!(name in self.SampleWindows))
        {
            self.SampleWindows[name] = new SampleWindow(self.WindowManager, name, self.NbSampleWindows);
            self.SampleWindows[name].WindowResized(self.SampleTimelineWindow.Window, self.Console.Window);
            self.NbSampleWindows++;
            MoveSampleWindows(this);
        }

        // Set on the window and timeline if connected as this implies a trace is being loaded, which we want to speed up
        if (self.Server.Connected())
        {
            self.SampleWindows[name].OnSamples(message.nb_samples, message.sample_digest, message.samples);
            self.SampleTimelineWindow.OnSamples(name, frame_history);
        }
    }


    function OnSampleName(self, socket, data_view_reader)
    {
        // Add any names sent by the server to the local map
        let name_hash = data_view_reader.GetUInt32();
        let name_string = data_view_reader.GetString();
        self.sampleNames.Set(name_hash, name_string);
    }


    function OnProcessorThreads(self, socket, data_view_reader)
    {
        let nb_processors = data_view_reader.GetUInt32();
        let message_index = data_view_reader.GetUInt64();

        // Decode each processor
        for (let i = 0; i < nb_processors; i++)
        {
            let thread_id = data_view_reader.GetUInt32();
            let thread_name_hash = data_view_reader.GetUInt32();
            let sample_time = data_view_reader.GetUInt64();

            // Add frame history for this processor
            let processor_name = "Processor " + i.toString();
            if (!(processor_name in self.ProcessorFrameHistory))
            {
                self.ProcessorFrameHistory[processor_name] = [ ];
            }
            let frame_history = self.ProcessorFrameHistory[processor_name];
            
            if (thread_id == 0xFFFFFFFF)
            {
                continue;
            }

            // Try to merge this frame's samples with the previous frame if the are the same thread
            if (frame_history.length > 0)
            {
                let last_thread_frame = frame_history[frame_history.length - 1];
                if (last_thread_frame.threadId == thread_id && last_thread_frame.messageIndex == message_index - 1)
                {
                    // Update last frame message index so that the next frame can check for continuity
                    last_thread_frame.messageIndex = message_index;

                    // Sum time elapsed on the previous frame
                    let us_length = sample_time - last_thread_frame.usLastStart;
                    last_thread_frame.usLastStart = sample_time;
                    last_thread_frame.EndTime_us += us_length;
                    last_thread_frame.Samples[0].us_length += us_length;

                    continue;
                }
            }
            
            // Discard old frames to keep memory-use constant
            var max_nb_frames = 10000;
            var extra_frames = frame_history.length - max_nb_frames;
            if (extra_frames > 0)
            {
                frame_history.splice(0, extra_frames);
            }
            
            // Lookup the thread name
            let [ _, thread_name ] = self.threadNames.Get(thread_name_hash);

            // Make a pastel-y colour from the thread name hash
            let hash = thread_name.hash;
            let r = 127 + (hash & 255) / 2;
            let g = 127 + ((hash >> 4) & 255) / 2;
            let b = 127 + ((hash >> 8) & 255) / 2;

            // We are co-opting the sample rendering functionality of the timeline window to display processor threads as
            // thread samples. Fabricate a thread frame message, packing the processor info into one root sample.
            // TODO(don): Abstract the timeline window for pure range display as this is quite inefficient.
            let thread_message = {
                nb_samples: 1,
                sample_digest: 0,
                samples : [
                    {
                        name_hash: thread_name_hash,
                        name: thread_name,
                        id: thread_id,
                        colour: "#FFFFFF",
                        us_start: sample_time,
                        us_length: 250,
                        rgbColour: [ r, g, b ],
                        children: []
                    }
                ]
            };

            // Create a thread frame and annotate with data required to merge processor samples
            let thread_frame = new ThreadFrame(thread_message);
            thread_frame.threadId = thread_id;
            thread_frame.messageIndex = message_index;
            thread_frame.usLastStart = sample_time;
            frame_history.push(thread_frame);

            if (self.Server.Connected())
            {
                self.ProcessorTimelineWindow.OnSamples(processor_name, frame_history);
            }
        }
    }


    function OnThreadNames(self, socket, data_view_reader)
    {
        let name_hash = data_view_reader.GetUInt32();
        let name_length = data_view_reader.GetUInt32();
        let name_string = data_view_reader.GetStringOfLength(name_length);
        self.threadNames.Set(name_hash, name_string);
    }


    function OnTimelineCheck(self, name, evt)
    {
        // Show/hide the equivalent sample window and move all the others to occupy any left-over space
        var target = DOM.Event.GetNode(evt);
        self.SampleWindows[name].SetVisible(target.checked);
        MoveSampleWindows(self);
    }


    function MoveSampleWindows(self)
    {
        // Stack all windows next to each other
        var xpos = 0;
        for (var i in self.SampleWindows)
        {
            var sample_window = self.SampleWindows[i];
            if (sample_window.Visible)
            {
                sample_window.SetXPos(xpos++, self.SampleTimelineWindow.Window, self.Console.Window);
            }
        }
    }


    function OnSampleHover(self, thread_name, hover)
    {
        if (!self.Settings.IsPaused)
        {
            return;
        }

        for (let window_thread_name in self.SampleWindows)
        {
            let sample_window = self.SampleWindows[window_thread_name];

            if (window_thread_name == thread_name && hover != null)
            {
                // Populate with sample under hover
                let frame = hover[0];
                sample_window.OnSamples(frame.NbSamples, frame.SampleDigest, frame.Samples);
            }
            else
            {
                // When there's no hover, go back to the selected frame
                if (self.SelectedFrames[window_thread_name])
                {
                    const frame = self.SelectedFrames[window_thread_name];
                    sample_window.OnSamples(frame.NbSamples, frame.SampleDigest, frame.Samples);
                }
            }
        }
    }


    function OnSampleSelected(self, thread_name, select)
    {
        // Lookup sample window set the frame samples on it
        if (select && thread_name in self.SampleWindows)
        {
            var sample_window = self.SampleWindows[thread_name];
            var frame = select[0];
            self.SelectedFrames[thread_name] = frame;
            sample_window.OnSamples(frame.NbSamples, frame.SampleDigest, frame.Samples);
        }
    }


    function OnResizeWindow(self)
    {
        // Resize windows
        var w = window.innerWidth;
        var h = window.innerHeight;
        self.Console.WindowResized(w, h);
        self.TitleWindow.WindowResized(w, h);
        self.SampleTimelineWindow.WindowResized(10, w / 2 - 5, self.TitleWindow.Window);
        self.ProcessorTimelineWindow.WindowResized(w / 2 + 5, w / 2 - 5, self.TitleWindow.Window);
        for (var i in self.SampleWindows)
        {
            self.SampleWindows[i].WindowResized(self.SampleTimelineWindow.Window, self.Console.Window);
        }
    }


    function OnTimelineMoved(self, timeline)
    {
        if (self.Settings.SyncTimelines)
        {
            let other_timeline = timeline == self.ProcessorTimelineWindow ? self.SampleTimelineWindow : self.ProcessorTimelineWindow;
            other_timeline.SetTimeRange(timeline.TimeRange.Start_us, timeline.TimeRange.Span_us);
            other_timeline.DrawAllRows();
        }
    }


    return Remotery;
})();
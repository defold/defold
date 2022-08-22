
TitleWindow = (function()
{
	function TitleWindow(wm, settings, server, connection_address)
	{
		this.Settings = settings;

		this.Window = wm.AddWindow("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Remotery", 10, 10, 100, 100);
		this.Window.ShowNoAnim();

		this.PingContainer = this.Window.AddControlNew(new WM.Container(4, -13, 10, 10));
		DOM.Node.AddClass(this.PingContainer.Node, "PingContainer");

		this.EditBox = this.Window.AddControlNew(new WM.EditBox(10, 5, 300, 18, "Connection Address", connection_address));

		// Setup pause button
		this.PauseButton = this.Window.AddControlNew(new WM.Button("Pause", 5, 5, { toggle: true }));
		this.PauseButton.SetOnClick(Bind(OnPausePressed, this));

		this.SyncButton = this.Window.AddControlNew(new WM.Button("Sync Timelines", 5, 5, { toggle: true}));
		this.SyncButton.SetOnClick(Bind(OnSyncPressed, this));
		this.SyncButton.SetState(this.Settings.SyncTimelines);

		server.AddMessageHandler("PING", Bind(OnPing, this));
		
		this.Window.SetOnResize(Bind(OnUserResize, this));
	}


	TitleWindow.prototype.SetConnectionAddressChanged = function(handler)
	{
		this.EditBox.SetChangeHandler(handler);
	}


	TitleWindow.prototype.WindowResized = function(width, height)
	{
		this.Window.SetSize(width - 2 * 10, 50);
		ResizeInternals(this);
	}

	TitleWindow.prototype.Pause = function()
	{
		if (!this.Settings.IsPaused)
		{
			this.PauseButton.SetText("Paused");
			this.PauseButton.SetState(true);
			this.Settings.IsPaused = true;
		}
	}

	TitleWindow.prototype.Unpause = function()
	{
		if (this.Settings.IsPaused)
		{
			this.PauseButton.SetText("Pause");
			this.PauseButton.SetState(false);
			this.Settings.IsPaused = false;
		}
	}

	function OnUserResize(self, evt)
	{
		ResizeInternals(self);
	}

	function ResizeInternals(self)
	{
		self.PauseButton.SetPosition(self.Window.Size[0] - 60, 5);
		self.SyncButton.SetPosition(self.Window.Size[0] - 155, 5);
	}


	function OnPausePressed(self)
	{
		if (self.PauseButton.IsPressed())
		{
			self.Pause();
		}
		else
		{
			self.Unpause();
		}
	}


	function OnSyncPressed(self)
	{
		self.Settings.SyncTimelines = self.SyncButton.IsPressed();
	}


	function OnPing(self, server)
	{
		// Set the ping container as active and take it off half a second later
		DOM.Node.AddClass(self.PingContainer.Node, "PingContainerActive");
		window.setTimeout(Bind(function(self)
		{
			DOM.Node.RemoveClass(self.PingContainer.Node, "PingContainerActive");
		}, self), 500);
	}


	return TitleWindow;
})();
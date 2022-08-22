
function GetTimeText(seconds)
{
    if (seconds < 0)
    {
        return "";
    }

    var text = "";

    // Add any contributing hours
    var h = Math.floor(seconds / 3600);
    seconds -= h * 3600;
    if (h)
    {
        text += h + "h ";
    }

    // Add any contributing minutes
    var m = Math.floor(seconds / 60);
    seconds -= m * 60;
    if (m)
    {
        text += m + "m ";
    }

    // Add any contributing seconds or always add seconds when hours or minutes have no contribution
    // This ensures the 0s marker displays
    var s = Math.floor(seconds);
    seconds -= s;
    if (s || text == "")
    {
        text += s + "s ";
    }
    
    // Add remaining milliseconds
    var ms = Math.floor(seconds * 1000);
    if (ms)
    {
        text += ms + "ms";
    }
    
    return text;
}


class TimelineMarkers
{
    constructor(timeline)
    {
        this.timeline = timeline;

        // Need a 2D drawing context
        this.markerContainer = timeline.Window.AddControlNew(new WM.Container(10, 10, 10, 10));
        this.markerCanvas = document.createElement("canvas");
        this.markerContainer.Node.appendChild(this.markerCanvas);
        this.markerContext = this.markerCanvas.getContext("2d");
    }

    Draw(time_range)
    {
        let ctx = this.markerContext;

        ctx.clearRect(0, 0, this.markerCanvas.width, this.markerCanvas.height);

            // Setup render state for the time line markers
        ctx.strokeStyle = "#BBB";
        ctx.fillStyle = "#BBB";
        ctx.lineWidth = 1;
        ctx.font = "9px LocalFiraCode";

        // A list of all supported units of time (measured in seconds) that require markers
        let units = [ 0.001, 0.01, 0.1, 1, 10, 60, 60 * 5, 60 * 60, 60 * 60 * 24 ];

        // Given the current pixel size of a second, calculate the spacing for each unit marker
        let second_pixel_size = time_range.PixelSize(1000 * 1000);
        let sizeof_units = [ ];
        for (let unit of units)
        {
            sizeof_units.push(unit * second_pixel_size);
        }

        // Calculate whether each unit marker is visible at the current zoom level
        var show_unit = [ ];
        for (let sizeof_unit of sizeof_units)
        {
            show_unit.push(Math.max(Math.min((sizeof_unit - 4) * 0.25, 1), 0));
        }

        // Find the first visible unit
        for (let i = 0; i < units.length; i++)
        {
            if (show_unit[i] > 0)
            {
                // Cut out unit information for the first set of units not visible
                units = units.slice(i);
                sizeof_units = sizeof_units.slice(i);
                show_unit = show_unit.slice(i);
                break;
            }
        }

        let timeline_end = this.markerCanvas.width;
        for (let i = 0; i < 3; i++)
        {
            // Round the start time up to the next visible unit
            let time_start = time_range.Start_us / (1000 * 1000);
            let unit_time_start = Math.ceil(time_start / units[i]) * units[i];

            // Calculate the canvas offset required to step to the first visible unit
            let pre_step_x = time_range.PixelOffset(unit_time_start * (1000 * 1000));

            // Draw lines for every unit at this level, keeping tracking of the seconds
            var seconds = unit_time_start;
            for (let x = pre_step_x; x <= timeline_end; x += sizeof_units[i])
            {
                // For the first two units, don't draw the units above it to prevent
                // overdraw and the visual errors that causes
                // The last unit always draws
                if (i > 1 || (seconds % units[i + 1]))
                {
                    // Only the first two units scale with unit visibility
                    // The last unit maintains its size
                    let height = Math.min(i * 4 + 4 * show_unit[i], 16);

                    // Draw the line on an integer boundary, shifted by 0.5 to get an un-anti-aliased 1px line
                    let ix = Math.floor(x);
                    ctx.beginPath();
                    ctx.moveTo(ix + 0.5, 1);
                    ctx.lineTo(ix + 0.5, 1 + height);
                    ctx.stroke();
                }

                seconds += units[i];
            }

            if (i == 1)
            {
                // Draw text labels for the second unit, fading them out as they slowly
                // become the first unit
                ctx.globalAlpha = show_unit[0];
                var seconds = unit_time_start;
                for (let x = pre_step_x; x <= timeline_end; x += sizeof_units[i])
                {
                    if (seconds % units[2])
                    {
                        this.DrawTimeText(seconds, x, 16);
                    }
                    seconds += units[i];
                }

                // Restore alpha
                ctx.globalAlpha = 1;
            }

            else if (i == 2)
            {
                // Draw text labels for the third unit with no fade
                var seconds = unit_time_start;
                for (let x = pre_step_x; x <= timeline_end; x += sizeof_units[i])
                {
                    this.DrawTimeText(seconds, x, 16);
                    seconds += units[i];
                }
            }
        }
    }

    DrawTimeText(seconds, x, y)
    {
        // Use text measuring to centre the text horizontally on the input x
        var text = GetTimeText(seconds);
        var width = this.markerContext.measureText(text).width;
        this.markerContext.fillText(text, Math.floor(x) - width / 2, y);
    }

    Resize(x, y, w, h)
    {
		this.markerContainer.SetPosition(x, y);
		this.markerContainer.SetSize(w, h);

        // Match canvas size to container
		this.markerCanvas.width = this.markerContainer.Node.clientWidth;
		this.markerCanvas.height = this.markerContainer.Node.clientHeight;
    }
}
package com.defold.editor.pipeline;

public class DistanceFieldGenerator
{
    public double[] lineSegments = new double[32768];
    public int lineSegmentsEnd = 0;

    public DistanceFieldGenerator()
    {

    }

    public void addLine(double x0, double y0, double x1, double y1)
    {
        lineSegments[lineSegmentsEnd+0] = x0;
        lineSegments[lineSegmentsEnd+1] = y0;
        lineSegments[lineSegmentsEnd+2] = x1 - x0;
        lineSegments[lineSegmentsEnd+3] = y1 - y0;
        lineSegments[lineSegmentsEnd+4] = 1.0 / ((x1-x0)*(x1-x0) + (y1-y0)*(y1-y0));
        lineSegmentsEnd += 5;
    }

    // Compute the minimal distance from [x, y] to any of the line segments
    public double distSqr(double x, double y)
    {
        double distMin = 10000000;
        for (int i=0;i<lineSegmentsEnd;i+=5)
        {
            double x0 = lineSegments[i];
            double y0 = lineSegments[i+1];
            double dx = lineSegments[i+2];
            double dy = lineSegments[i+3];
            double k = lineSegments[i+4];

            double dx0 = x - x0;
            double dy0 = y - y0;
            double t = k * (dx * dx0 + dy * dy0);

            if (t < 0)
            {
                // Closest point is t=0 of the line
                double distSqr = dx0 * dx0 + dy0 * dy0;
                if (distSqr < distMin)
                    distMin = distSqr;
            }
            else if (t > 1)
            {
                // Closest point is t=1 of the line
                double xx = x - (x0 + dx);
                double yy = y - (y0 + dy);
                double distSqr = xx*xx + yy*yy;
                if (distSqr < distMin)
                    distMin = distSqr;
            }
            else
            {
                // Case when the closest point is along the line, and t will be [0,1]
                double px = x0 + t * dx - x;
                double py = y0 + t * dy - y;
                double distSqr = px*px + py*py;
                if (distSqr < distMin)
                    distMin = distSqr;
            }
        }
        return distMin;
    }

    public void render(double[] output, double x0, double y0, double x1, double y1, int width, int height)
    {
        int ofs = 0;
        double dx = (x1 - x0) / (double)width;
        for (int y=0;y<height;y++)
        {
            double py = y0 + y * (y1-y0) / (double)height;
            double px = x0;
            for (int x=0;x<width;x++)
            {
                output[ofs++] = Math.sqrt(distSqr(px, py));
                px += dx;
            }
        }
    }
}

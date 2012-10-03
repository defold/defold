package com.dynamo.cr.parted.nodes;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import com.dynamo.cr.parted.curve.HermiteSpline;
import com.dynamo.cr.properties.types.ValueSpread;
import com.dynamo.particle.proto.Particle;
import com.dynamo.particle.proto.Particle.SplinePoint;

public class ParticleUtils {

    public static ValueSpread toValueSpread(List<SplinePoint> pl) {
        int count = pl.size() * 4;
        boolean animated = pl.size() > 1;

        if (!animated) {
            count += 4;
        }

        float[] data = new float[count];
        int i = 0;
        for (SplinePoint sp : pl) {
            data[i++] = sp.getX();
            data[i++] = sp.getY();
            data[i++] = sp.getTX();
            data[i++] = sp.getTY();
        }

        ValueSpread vs = new ValueSpread();
        if (!animated) {
            SplinePoint sp = pl.get(0);
            data[i++] = 1;
            data[i++] = sp.getY();
            data[i++] = sp.getTX();
            data[i++] = sp.getTY();
            vs.setValue(sp.getY());
        }

        HermiteSpline spline = new HermiteSpline(data);
        vs.setCurve(spline);
        vs.setAnimated(animated);
        vs.setValue(pl.get(0).getY());
        return vs;
    }

    public static List<SplinePoint> toSplinePointList(ValueSpread valueSpread) {
        HermiteSpline spline = (HermiteSpline) valueSpread.getCurve();
        if (valueSpread.isAnimated()) {
            List<SplinePoint> lst = new ArrayList<Particle.SplinePoint>();
            for (int i = 0; i < spline.getCount(); ++i) {
                com.dynamo.cr.parted.curve.SplinePoint p = spline.getPoint(i);
                SplinePoint sp = SplinePoint.newBuilder()
                        .setX((float) p.getX())
                        .setY((float) p.getY())
                        .setTX((float) p.getTx())
                        .setTY((float) p.getTy())
                        .build();

                lst.add(sp);
            }
            return lst;
        } else {
            return Arrays.asList(SplinePoint.newBuilder()
                    .setX(0)
                    .setY((float) valueSpread.getValue())
                    .setTX(1)
                    .setTY(0)
                    .build());

        }
    }
}

#include <stdio.h>
#include <iomanip>
#include <QMetaEnum>
#include <QEasingCurve>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QString>
#include <QFile>
#include <QTextStream>

/*
 * Generates lookup-table header and json-file with easing-data
 * using the class QEasingCurve in QT.
 *
 * Compile with something like this on OSX:
 * g++ -I ~/Qt5.0.0/5.0.0/clang_64/include/QtCore -I ~/Qt5.0.0/5.0.0/clang_64/include/ -F ~/Qt5.0.0/5.0.0/clang_64/lib  scripts/gen_easing.cpp  -framework QtCore
 */

int main(int argc, char **argv)
{
    // NOTE: The last sample is duplicated to simplify interpolation logic
    const int n_samples = 64;
    const int max_curve_index = 40;

    QMetaEnum en = QEasingCurve::staticMetaObject.enumerator(0);

    QFile json_file("easing.json");
    json_file.open(QIODevice::WriteOnly);
    QTextStream json_stream(&json_file);

    QFile lookup_file("src/dlib/easing_lookup.h");
    lookup_file.open(QIODevice::WriteOnly);
    QTextStream lookup_stream(&lookup_file);
    lookup_stream.setRealNumberPrecision(8);
    lookup_stream.setRealNumberNotation(QTextStream::FixedNotation);

    lookup_stream << "// NOTE: GENERATED FILE. DO NOT EDIT\n\n";
    lookup_stream << "/*\n";
    lookup_stream << "enum Type\n";
    lookup_stream << "{\n";
    for (int ct = 0; ct <= max_curve_index; ++ct) {
        lookup_stream << "    TYPE_" << QString(en.key(ct)).toUpper() << " = " << ct << ",\n";
    }
    lookup_stream << "    TYPE_COUNT" << " = " << max_curve_index+1 << ",\n";
    lookup_stream << "};\n";
    lookup_stream << "*/\n";

    lookup_stream << "const int EASING_SAMPLES = " << n_samples << ";\n";

    lookup_stream << "float EASING_LOOKUP[] = { \n";

    QJsonArray curves;
    for (int ct = 0; ct <= max_curve_index; ++ct) {
        QEasingCurve c((QEasingCurve::Type) ct);
        QJsonArray values;
        lookup_stream << "    // " << QString(en.key(ct)) << "\n";
        // NOTE: +1 to duplicate last sample
        for (int i = 0; i < n_samples+1; ++i) {
            qreal t = i / qreal(n_samples-1);
            if (t > 1)
                t = 1;
            qreal val = c.valueForProgress(t);
            values.append(val);
            lookup_stream << "    " << val << "f,\n";
        }
        QJsonObject o;
        o["type"] = QString(en.key(ct));
        o["values"] = values;
        curves.append(o);
    }
    lookup_stream << "};\n";

    QJsonDocument d(curves);
    json_stream << d.toJson();
    json_stream.flush();
}

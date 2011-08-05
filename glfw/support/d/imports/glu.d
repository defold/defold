/************************************************************************
 *
 * GLU 1.0 - 1.3 module for the D programming language
 * (http://www.digitalmars.com/d/)
 *
 * Conversion by Marcus Geelnard
 *
 * Originally based on glu.h from the SGI Open Source Sample
 * Implementation.
 *
 ************************************************************************/

import gl;


version (Win32) {
    extern (Windows):
}
version (linux) {
    extern (C):
}


/************************************************************************
 *
 * GLU versions supported by this module (1.0 is assumed, and requires
 * no further introduction)
 *
 ************************************************************************/

const uint GLU_VERSION_1_1                    = 1;
const uint GLU_VERSION_1_2                    = 1;
const uint GLU_VERSION_1_3                    = 1;



/************************************************************************
 *
 * Datatypes
 *
 ************************************************************************/

alias void GLUnurbs;
alias void GLUquadric;
alias void GLUtesselator;

alias GLUnurbs      GLUnurbsObj;
alias GLUquadric    GLUquadricObj;
alias GLUtesselator GLUtesselatorObj;
alias GLUtesselator GLUtriangulatorObj;

/* Internal convenience typedef */
typedef void (* _GLUfuncptr)();



/************************************************************************
 *
 * Constants
 *
 ************************************************************************/

/* Boolean */
const uint GLU_FALSE                          = 0;
const uint GLU_TRUE                           = 1;


/* StringName */
const uint GLU_VERSION                        = 100800;
const uint GLU_EXTENSIONS                     = 100801;

/* ErrorCode */
const uint GLU_INVALID_ENUM                   = 100900;
const uint GLU_INVALID_VALUE                  = 100901;
const uint GLU_OUT_OF_MEMORY                  = 100902;
const uint GLU_INVALID_OPERATION              = 100904;

/* NurbsDisplay */
/*      GLU_FILL */
const uint GLU_OUTLINE_POLYGON                = 100240;
const uint GLU_OUTLINE_PATCH                  = 100241;

/* NurbsCallback */
const uint GLU_NURBS_ERROR                    = 100103;
const uint GLU_ERROR                          = 100103;
const uint GLU_NURBS_BEGIN                    = 100164;
const uint GLU_NURBS_BEGIN_EXT                = 100164;
const uint GLU_NURBS_VERTEX                   = 100165;
const uint GLU_NURBS_VERTEX_EXT               = 100165;
const uint GLU_NURBS_NORMAL                   = 100166;
const uint GLU_NURBS_NORMAL_EXT               = 100166;
const uint GLU_NURBS_COLOR                    = 100167;
const uint GLU_NURBS_COLOR_EXT                = 100167;
const uint GLU_NURBS_TEXTURE_COORD            = 100168;
const uint GLU_NURBS_TEX_COORD_EXT            = 100168;
const uint GLU_NURBS_END                      = 100169;
const uint GLU_NURBS_END_EXT                  = 100169;
const uint GLU_NURBS_BEGIN_DATA               = 100170;
const uint GLU_NURBS_BEGIN_DATA_EXT           = 100170;
const uint GLU_NURBS_VERTEX_DATA              = 100171;
const uint GLU_NURBS_VERTEX_DATA_EXT          = 100171;
const uint GLU_NURBS_NORMAL_DATA              = 100172;
const uint GLU_NURBS_NORMAL_DATA_EXT          = 100172;
const uint GLU_NURBS_COLOR_DATA               = 100173;
const uint GLU_NURBS_COLOR_DATA_EXT           = 100173;
const uint GLU_NURBS_TEXTURE_COORD_DATA       = 100174;
const uint GLU_NURBS_TEX_COORD_DATA_EXT       = 100174;
const uint GLU_NURBS_END_DATA                 = 100175;
const uint GLU_NURBS_END_DATA_EXT             = 100175;

/* NurbsError */
const uint GLU_NURBS_ERROR1                   = 100251;
const uint GLU_NURBS_ERROR2                   = 100252;
const uint GLU_NURBS_ERROR3                   = 100253;
const uint GLU_NURBS_ERROR4                   = 100254;
const uint GLU_NURBS_ERROR5                   = 100255;
const uint GLU_NURBS_ERROR6                   = 100256;
const uint GLU_NURBS_ERROR7                   = 100257;
const uint GLU_NURBS_ERROR8                   = 100258;
const uint GLU_NURBS_ERROR9                   = 100259;
const uint GLU_NURBS_ERROR10                  = 100260;
const uint GLU_NURBS_ERROR11                  = 100261;
const uint GLU_NURBS_ERROR12                  = 100262;
const uint GLU_NURBS_ERROR13                  = 100263;
const uint GLU_NURBS_ERROR14                  = 100264;
const uint GLU_NURBS_ERROR15                  = 100265;
const uint GLU_NURBS_ERROR16                  = 100266;
const uint GLU_NURBS_ERROR17                  = 100267;
const uint GLU_NURBS_ERROR18                  = 100268;
const uint GLU_NURBS_ERROR19                  = 100269;
const uint GLU_NURBS_ERROR20                  = 100270;
const uint GLU_NURBS_ERROR21                  = 100271;
const uint GLU_NURBS_ERROR22                  = 100272;
const uint GLU_NURBS_ERROR23                  = 100273;
const uint GLU_NURBS_ERROR24                  = 100274;
const uint GLU_NURBS_ERROR25                  = 100275;
const uint GLU_NURBS_ERROR26                  = 100276;
const uint GLU_NURBS_ERROR27                  = 100277;
const uint GLU_NURBS_ERROR28                  = 100278;
const uint GLU_NURBS_ERROR29                  = 100279;
const uint GLU_NURBS_ERROR30                  = 100280;
const uint GLU_NURBS_ERROR31                  = 100281;
const uint GLU_NURBS_ERROR32                  = 100282;
const uint GLU_NURBS_ERROR33                  = 100283;
const uint GLU_NURBS_ERROR34                  = 100284;
const uint GLU_NURBS_ERROR35                  = 100285;
const uint GLU_NURBS_ERROR36                  = 100286;
const uint GLU_NURBS_ERROR37                  = 100287;

/* NurbsProperty */
const uint GLU_AUTO_LOAD_MATRIX               = 100200;
const uint GLU_CULLING                        = 100201;
const uint GLU_SAMPLING_TOLERANCE             = 100203;
const uint GLU_DISPLAY_MODE                   = 100204;
const uint GLU_PARAMETRIC_TOLERANCE           = 100202;
const uint GLU_SAMPLING_METHOD                = 100205;
const uint GLU_U_STEP                         = 100206;
const uint GLU_V_STEP                         = 100207;
const uint GLU_NURBS_MODE                     = 100160;
const uint GLU_NURBS_MODE_EXT                 = 100160;
const uint GLU_NURBS_TESSELLATOR              = 100161;
const uint GLU_NURBS_TESSELLATOR_EXT          = 100161;
const uint GLU_NURBS_RENDERER                 = 100162;
const uint GLU_NURBS_RENDERER_EXT             = 100162;

/* NurbsSampling */
const uint GLU_OBJECT_PARAMETRIC_ERROR        = 100208;
const uint GLU_OBJECT_PARAMETRIC_ERROR_EXT    = 100208;
const uint GLU_OBJECT_PATH_LENGTH             = 100209;
const uint GLU_OBJECT_PATH_LENGTH_EXT         = 100209;
const uint GLU_PATH_LENGTH                    = 100215;
const uint GLU_PARAMETRIC_ERROR               = 100216;
const uint GLU_DOMAIN_DISTANCE                = 100217;

/* NurbsTrim */
const uint GLU_MAP1_TRIM_2                    = 100210;
const uint GLU_MAP1_TRIM_3                    = 100211;

/* QuadricDrawStyle */
const uint GLU_POINT                          = 100010;
const uint GLU_LINE                           = 100011;
const uint GLU_FILL                           = 100012;
const uint GLU_SILHOUETTE                     = 100013;

/* QuadricCallback */
/*      GLU_ERROR */

/* QuadricNormal */
const uint GLU_SMOOTH                         = 100000;
const uint GLU_FLAT                           = 100001;
const uint GLU_NONE                           = 100002;

/* QuadricOrientation */
const uint GLU_OUTSIDE                        = 100020;
const uint GLU_INSIDE                         = 100021;

/* TessCallback */
const uint GLU_TESS_BEGIN                     = 100100;
const uint GLU_BEGIN                          = 100100;
const uint GLU_TESS_VERTEX                    = 100101;
const uint GLU_VERTEX                         = 100101;
const uint GLU_TESS_END                       = 100102;
const uint GLU_END                            = 100102;
const uint GLU_TESS_ERROR                     = 100103;
const uint GLU_TESS_EDGE_FLAG                 = 100104;
const uint GLU_EDGE_FLAG                      = 100104;
const uint GLU_TESS_COMBINE                   = 100105;
const uint GLU_TESS_BEGIN_DATA                = 100106;
const uint GLU_TESS_VERTEX_DATA               = 100107;
const uint GLU_TESS_END_DATA                  = 100108;
const uint GLU_TESS_ERROR_DATA                = 100109;
const uint GLU_TESS_EDGE_FLAG_DATA            = 100110;
const uint GLU_TESS_COMBINE_DATA              = 100111;

/* TessContour */
const uint GLU_CW                             = 100120;
const uint GLU_CCW                            = 100121;
const uint GLU_INTERIOR                       = 100122;
const uint GLU_EXTERIOR                       = 100123;
const uint GLU_UNKNOWN                        = 100124;

/* TessProperty */
const uint GLU_TESS_WINDING_RULE              = 100140;
const uint GLU_TESS_BOUNDARY_ONLY             = 100141;
const uint GLU_TESS_TOLERANCE                 = 100142;

/* TessError */
const uint GLU_TESS_ERROR1                    = 100151;
const uint GLU_TESS_ERROR2                    = 100152;
const uint GLU_TESS_ERROR3                    = 100153;
const uint GLU_TESS_ERROR4                    = 100154;
const uint GLU_TESS_ERROR5                    = 100155;
const uint GLU_TESS_ERROR6                    = 100156;
const uint GLU_TESS_ERROR7                    = 100157;
const uint GLU_TESS_ERROR8                    = 100158;
const uint GLU_TESS_MISSING_BEGIN_POLYGON     = 100151;
const uint GLU_TESS_MISSING_BEGIN_CONTOUR     = 100152;
const uint GLU_TESS_MISSING_END_POLYGON       = 100153;
const uint GLU_TESS_MISSING_END_CONTOUR       = 100154;
const uint GLU_TESS_COORD_TOO_LARGE           = 100155;
const uint GLU_TESS_NEED_COMBINE_CALLBACK     = 100156;

/* TessWinding */
const uint GLU_TESS_WINDING_ODD               = 100130;
const uint GLU_TESS_WINDING_NONZERO           = 100131;
const uint GLU_TESS_WINDING_POSITIVE          = 100132;
const uint GLU_TESS_WINDING_NEGATIVE          = 100133;
const uint GLU_TESS_WINDING_ABS_GEQ_TWO       = 100134;


/************************************************************************
 *
 * Extensions
 *
 ************************************************************************/

const uint GLU_EXT_object_space_tess          = 1;
const uint GLU_EXT_nurbs_tessellator          = 1;

const double GLU_TESS_MAX_COORD               = 1.0e150;



/************************************************************************
 *
 * Function prototypes
 *
 ************************************************************************/

void gluBeginCurve (GLUnurbs* nurb);
void gluBeginPolygon (GLUtesselator* tess);
void gluBeginSurface (GLUnurbs* nurb);
void gluBeginTrim (GLUnurbs* nurb);
GLint gluBuild1DMipmapLevels (GLenum target, GLint internalFormat, GLsizei width, GLenum format, GLenum type, GLint level, GLint base, GLint max, void *data);
GLint gluBuild1DMipmaps (GLenum target, GLint internalFormat, GLsizei width, GLenum format, GLenum type, void *data);
GLint gluBuild2DMipmapLevels (GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, GLint level, GLint base, GLint max, void *data);
GLint gluBuild2DMipmaps (GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, void *data);
GLint gluBuild3DMipmapLevels (GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, GLint level, GLint base, GLint max, void *data);
GLint gluBuild3DMipmaps (GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, void *data);
GLboolean gluCheckExtension (GLubyte *extName, GLubyte *extString);
void gluCylinder (GLUquadric* quad, GLdouble base, GLdouble top, GLdouble height, GLint slices, GLint stacks);
void gluDeleteNurbsRenderer (GLUnurbs* nurb);
void gluDeleteQuadric (GLUquadric* quad);
void gluDeleteTess (GLUtesselator* tess);
void gluDisk (GLUquadric* quad, GLdouble inner, GLdouble outer, GLint slices, GLint loops);
void gluEndCurve (GLUnurbs* nurb);
void gluEndPolygon (GLUtesselator* tess);
void gluEndSurface (GLUnurbs* nurb);
void gluEndTrim (GLUnurbs* nurb);
GLubyte * gluErrorString (GLenum error);
void gluGetNurbsProperty (GLUnurbs* nurb, GLenum property, GLfloat* data);
GLubyte * gluGetString (GLenum name);
void gluGetTessProperty (GLUtesselator* tess, GLenum which, GLdouble* data);
void gluLoadSamplingMatrices (GLUnurbs* nurb, GLfloat *model, GLfloat *perspective, GLint *view);
void gluLookAt (GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ, GLdouble centerX, GLdouble centerY, GLdouble centerZ, GLdouble upX, GLdouble upY, GLdouble upZ);
GLUnurbs* gluNewNurbsRenderer ();
GLUquadric* gluNewQuadric ();
GLUtesselator* gluNewTess ();
void gluNextContour (GLUtesselator* tess, GLenum type);
void gluNurbsCallback (GLUnurbs* nurb, GLenum which, _GLUfuncptr CallBackFunc);
void gluNurbsCallbackData (GLUnurbs* nurb, GLvoid* userData);
void gluNurbsCallbackDataEXT (GLUnurbs* nurb, GLvoid* userData);
void gluNurbsCurve (GLUnurbs* nurb, GLint knotCount, GLfloat *knots, GLint stride, GLfloat *control, GLint order, GLenum type);
void gluNurbsProperty (GLUnurbs* nurb, GLenum property, GLfloat value);
void gluNurbsSurface (GLUnurbs* nurb, GLint sKnotCount, GLfloat* sKnots, GLint tKnotCount, GLfloat* tKnots, GLint sStride, GLint tStride, GLfloat* control, GLint sOrder, GLint tOrder, GLenum type);
void gluOrtho2D (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top);
void gluPartialDisk (GLUquadric* quad, GLdouble inner, GLdouble outer, GLint slices, GLint loops, GLdouble start, GLdouble sweep);
void gluPerspective (GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar);
void gluPickMatrix (GLdouble x, GLdouble y, GLdouble delX, GLdouble delY, GLint *viewport);
GLint gluProject (GLdouble objX, GLdouble objY, GLdouble objZ, GLdouble *model, GLdouble *proj, GLint *view, GLdouble* winX, GLdouble* winY, GLdouble* winZ);
void gluPwlCurve (GLUnurbs* nurb, GLint count, GLfloat* data, GLint stride, GLenum type);
void gluQuadricCallback (GLUquadric* quad, GLenum which, _GLUfuncptr CallBackFunc);
void gluQuadricDrawStyle (GLUquadric* quad, GLenum draw);
void gluQuadricNormals (GLUquadric* quad, GLenum normal);
void gluQuadricOrientation (GLUquadric* quad, GLenum orientation);
void gluQuadricTexture (GLUquadric* quad, GLboolean texture);
GLint gluScaleImage (GLenum format, GLsizei wIn, GLsizei hIn, GLenum typeIn, void *dataIn, GLsizei wOut, GLsizei hOut, GLenum typeOut, GLvoid* dataOut);
void gluSphere (GLUquadric* quad, GLdouble radius, GLint slices, GLint stacks);
void gluTessBeginContour (GLUtesselator* tess);
void gluTessBeginPolygon (GLUtesselator* tess, GLvoid* data);
void gluTessCallback (GLUtesselator* tess, GLenum which, _GLUfuncptr CallBackFunc);
void gluTessEndContour (GLUtesselator* tess);
void gluTessEndPolygon (GLUtesselator* tess);
void gluTessNormal (GLUtesselator* tess, GLdouble valueX, GLdouble valueY, GLdouble valueZ);
void gluTessProperty (GLUtesselator* tess, GLenum which, GLdouble data);
void gluTessVertex (GLUtesselator* tess, GLdouble *location, GLvoid* data);
GLint gluUnProject (GLdouble winX, GLdouble winY, GLdouble winZ, GLdouble *model, GLdouble *proj, GLint *view, GLdouble* objX, GLdouble* objY, GLdouble* objZ);
GLint gluUnProject4 (GLdouble winX, GLdouble winY, GLdouble winZ, GLdouble clipW, GLdouble *model, GLdouble *proj, GLint *view, GLdouble nearVal, GLdouble farVal, GLdouble* objX, GLdouble* objY, GLdouble* objZ, GLdouble* objW);

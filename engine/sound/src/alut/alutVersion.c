#include "alutInternal.h"

ALUT_API ALint
ALUT_APIENTRY alutGetMajorVersion (void)
{
  return ALUT_API_MAJOR_VERSION;
}

ALUT_API ALint
ALUT_APIENTRY alutGetMinorVersion (void)
{
  return ALUT_API_MINOR_VERSION;
}

from .helper import *


def GenMemMap(mod, dir):
    H = open("%s/%s_MemMap.h" % (dir, mod), "w")
    GenHeader(H)
    H.write(
        """
#ifdef USE_MEMMAP
#ifdef {0}_START_SEC_CONST
#undef {0}_START_SEC_CONST
#if defined(__HIWARE__)
#pragma CONST_SEG __FAR_SEG.{0}
#endif
#if defined(__CSP__)
#pragma section const "{0}"
#endif
#if defined(__ghs__)
#pragma ghs section rosdata=".{0}"
#endif
#endif

#ifdef {0}_STOP_SEC_CONST
#undef {0}_STOP_SEC_CONST
#if defined(__HIWARE__)
#pragma CONST_SEG DEFAULT
#endif
#if defined(__CSP__)
#pragma section default
#endif
#if defined(__ghs__)
#pragma ghs section rosdata=default
#endif
#endif
#endif /* USE_MEMMAP*/

""".format(
            mod.upper()
        )
    )
    H.close()

//
// da_types.h
//

#ifndef da_types_h
#define da_types_h

#ifndef NULL
// #include <_null.h>  // removed by sch
#define NULL 0
#endif

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned short ushort;
typedef unsigned char uchar;

enum ErrorCode
{
        ErrRANGE, ErrMEM, ErrNULLPTR, ErrSIZE, ErrCOPY
};

void FatalError(ErrorCode ec);
#endif

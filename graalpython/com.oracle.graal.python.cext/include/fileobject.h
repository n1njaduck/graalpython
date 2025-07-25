/* Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 * Copyright (C) 1996-2020 Python Software Foundation
 *
 * Licensed under the PYTHON SOFTWARE FOUNDATION LICENSE VERSION 2
 */
/* File object interface (what's left of it -- see io.py) */

#ifndef Py_FILEOBJECT_H
#define Py_FILEOBJECT_H
#ifdef __cplusplus
extern "C" {
#endif

#define PY_STDIOTEXTMODE "b"

PyAPI_FUNC(PyObject *) PyFile_FromFd(int, const char *, const char *, int,
                                     const char *, const char *,
                                     const char *, int);
PyAPI_FUNC(PyObject *) PyFile_GetLine(PyObject *, int);
PyAPI_FUNC(int) PyFile_WriteObject(PyObject *, PyObject *, int);
PyAPI_FUNC(int) PyFile_WriteString(const char *, PyObject *);
PyAPI_FUNC(int) PyObject_AsFileDescriptor(PyObject *);

/* The default encoding used by the platform file system APIs
   If non-NULL, this is different than the default encoding for strings
*/
Py_DEPRECATED(3.12) PyAPI_DATA(const char *) Py_FileSystemDefaultEncoding;
#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03060000
Py_DEPRECATED(3.12) PyAPI_DATA(const char *) Py_FileSystemDefaultEncodeErrors;
#endif
Py_DEPRECATED(3.12) PyAPI_DATA(int) Py_HasFileSystemDefaultEncoding;

#if !defined(Py_LIMITED_API) || Py_LIMITED_API+0 >= 0x03070000
Py_DEPRECATED(3.12) PyAPI_DATA(int) Py_UTF8Mode;
#endif

/* A routine to check if a file descriptor can be select()-ed. */
#ifdef _MSC_VER
    /* On Windows, any socket fd can be select()-ed, no matter how high */
    #define _PyIsSelectable_fd(FD) (1)
#else
    #define _PyIsSelectable_fd(FD) ((unsigned int)(FD) < (unsigned int)FD_SETSIZE)
#endif

#ifndef Py_LIMITED_API
#  define Py_CPYTHON_FILEOBJECT_H
#  include "cpython/fileobject.h"
#  undef Py_CPYTHON_FILEOBJECT_H
#endif

#ifdef __cplusplus
}
#endif
#endif /* !Py_FILEOBJECT_H */

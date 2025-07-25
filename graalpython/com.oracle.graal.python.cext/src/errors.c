/* Copyright (c) 2024, 2025, Oracle and/or its affiliates.
 * Copyright (C) 1996-2024 Python Software Foundation
 *
 * Licensed under the PYTHON SOFTWARE FOUNDATION LICENSE VERSION 2
 */

/* Error handling */

#include "capi.h" // GraalPy change
#include "Python.h"
#include "pycore_call.h"          // _PyObject_CallNoArgs()
#if 0 // GraalPy change
#include "pycore_initconfig.h"    // _PyStatus_ERR()
#endif // GraalPy change
#include "pycore_pyerrors.h"      // _PyErr_Format()
#include "pycore_pystate.h"       // _PyThreadState_GET()
#if 0 // GraalPy change
#include "pycore_structseq.h"     // _PyStructSequence_FiniBuiltin()
#include "pycore_sysmodule.h"     // _PySys_Audit()
#include "pycore_traceback.h"     // _PyTraceBack_FromFrame()
#endif // GraalPy change

#include <ctype.h>
#ifdef MS_WINDOWS
#  include <windows.h>
#  include <winbase.h>
#  include <stdlib.h>             // _sys_nerr
#endif


#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
static PyObject *
_PyErr_FormatV(PyThreadState *tstate, PyObject *exception,
               const char *format, va_list vargs);

void
_PyErr_SetRaisedException(PyThreadState *tstate, PyObject *exc)
{
    PyObject *old_exc = tstate->current_exception;
    tstate->current_exception = exc;
    Py_XDECREF(old_exc);
}

static PyObject*
_PyErr_CreateException(PyObject *exception_type, PyObject *value)
{
    PyObject *exc;

    if (value == NULL || value == Py_None) {
        exc = _PyObject_CallNoArgs(exception_type);
    }
    else if (PyTuple_Check(value)) {
        exc = PyObject_Call(exception_type, value, NULL);
    }
    else {
        exc = PyObject_CallOneArg(exception_type, value);
    }

    if (exc != NULL && !PyExceptionInstance_Check(exc)) {
        PyErr_Format(PyExc_TypeError,
                     "calling %R should have returned an instance of "
                     "BaseException, not %s",
                     exception_type, Py_TYPE(exc)->tp_name);
        Py_CLEAR(exc);
    }

    return exc;
}

void
_PyErr_Restore(PyThreadState *tstate, PyObject *type, PyObject *value,
               PyObject *traceback)
{
    if (type == NULL) {
        assert(value == NULL);
        assert(traceback == NULL);
        _PyErr_SetRaisedException(tstate, NULL);
        return;
    }
    assert(PyExceptionClass_Check(type));
    if (value != NULL && type == (PyObject *)Py_TYPE(value)) {
        /* Already normalized */
        // GraalPy change
        // assert(((PyBaseExceptionObject *)value)->traceback != Py_None);
    }
    else {
        PyObject *exc = _PyErr_CreateException(type, value);
        Py_XDECREF(value);
        if (exc == NULL) {
            Py_DECREF(type);
            Py_XDECREF(traceback);
            return;
        }
        value = exc;
    }
    assert(PyExceptionInstance_Check(value));
    if (traceback != NULL && !PyTraceBack_Check(traceback)) {
        if (traceback == Py_None) {
            Py_DECREF(Py_None);
            traceback = NULL;
        }
        else {
            PyErr_SetString(PyExc_TypeError, "traceback must be a Traceback or None");
            Py_XDECREF(value);
            Py_DECREF(type);
            Py_XDECREF(traceback);
            return;
        }
    }
    // GraalPy change: upcall for setting the traceback
    GraalPyTruffleErr_SetTraceback(value, traceback);
    // GraalPy change: the traceback is now owned by the managed side
    Py_XDECREF(traceback);
    _PyErr_SetRaisedException(tstate, value);
    Py_DECREF(type);
}

void
PyErr_Restore(PyObject *type, PyObject *value, PyObject *traceback)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_Restore(tstate, type, value, traceback);
}

void
PyErr_SetRaisedException(PyObject *exc)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetRaisedException(tstate, exc);
}

#if 0 // GraalPy change
_PyErr_StackItem *
_PyErr_GetTopmostException(PyThreadState *tstate)
{
    _PyErr_StackItem *exc_info = tstate->exc_info;
    assert(exc_info);

    while ((exc_info->exc_value == NULL || exc_info->exc_value == Py_None) &&
           exc_info->previous_item != NULL)
    {
        exc_info = exc_info->previous_item;
    }
    return exc_info;
}

static PyObject *
get_normalization_failure_note(PyThreadState *tstate, PyObject *exception, PyObject *value)
{
    PyObject *args = PyObject_Repr(value);
    if (args == NULL) {
        _PyErr_Clear(tstate);
        args = PyUnicode_FromFormat("<unknown>");
    }
    PyObject *note;
    const char *tpname = ((PyTypeObject*)exception)->tp_name;
    if (args == NULL) {
        _PyErr_Clear(tstate);
        note = PyUnicode_FromFormat("Normalization failed: type=%s", tpname);
    }
    else {
        note = PyUnicode_FromFormat("Normalization failed: type=%s args=%S",
                                    tpname, args);
        Py_DECREF(args);
    }
    return note;
}
#endif // GraalPy change

void
_PyErr_SetObject(PyThreadState *tstate, PyObject *exception, PyObject *value)
{
#if 0 // GraalPy change
    PyObject *exc_value;
    PyObject *tb = NULL;

    if (exception != NULL &&
        !PyExceptionClass_Check(exception)) {
        _PyErr_Format(tstate, PyExc_SystemError,
                      "_PyErr_SetObject: "
                      "exception %R is not a BaseException subclass",
                      exception);
        return;
    }
    /* Normalize the exception */
    int is_subclass = 0;
    if (value != NULL && PyExceptionInstance_Check(value)) {
        is_subclass = PyObject_IsSubclass((PyObject *)Py_TYPE(value), exception);
        if (is_subclass < 0) {
            return;
        }
    }
    Py_XINCREF(value);
    if (!is_subclass) {
        /* We must normalize the value right now */

        /* Issue #23571: functions must not be called with an
            exception set */
        _PyErr_Clear(tstate);

        PyObject *fixed_value = _PyErr_CreateException(exception, value);
        if (fixed_value == NULL) {
            PyObject *exc = _PyErr_GetRaisedException(tstate);
            assert(PyExceptionInstance_Check(exc));

            PyObject *note = get_normalization_failure_note(tstate, exception, value);
            Py_XDECREF(value);
            if (note != NULL) {
                /* ignore errors in _PyException_AddNote - they will be overwritten below */
                _PyException_AddNote(exc, note);
                Py_DECREF(note);
            }
            _PyErr_SetRaisedException(tstate, exc);
            return;
        }
        Py_XSETREF(value, fixed_value);
    }

    exc_value = _PyErr_GetTopmostException(tstate)->exc_value;
    if (exc_value != NULL && exc_value != Py_None) {
        /* Implicit exception chaining */
        Py_INCREF(exc_value);
        /* Avoid creating new reference cycles through the
           context chain, while taking care not to hang on
           pre-existing ones.
           This is O(chain length) but context chains are
           usually very short. Sensitive readers may try
           to inline the call to PyException_GetContext. */
        if (exc_value != value) {
            PyObject *o = exc_value, *context;
            PyObject *slow_o = o;  /* Floyd's cycle detection algo */
            int slow_update_toggle = 0;
            while ((context = PyException_GetContext(o))) {
                Py_DECREF(context);
                if (context == value) {
                    PyException_SetContext(o, NULL);
                    break;
                }
                o = context;
                if (o == slow_o) {
                    /* pre-existing cycle - all exceptions on the
                       path were visited and checked.  */
                    break;
                }
                if (slow_update_toggle) {
                    slow_o = PyException_GetContext(slow_o);
                    Py_DECREF(slow_o);
                }
                slow_update_toggle = !slow_update_toggle;
            }
            PyException_SetContext(value, exc_value);
        }
        else {
            Py_DECREF(exc_value);
        }
    }
    assert(value != NULL);
    if (PyExceptionInstance_Check(value))
        tb = PyException_GetTraceback(value);
    _PyErr_Restore(tstate, Py_NewRef(Py_TYPE(value)), value, tb);
#else // GraalPy change: different implementation
	Graal_PyTruffleErr_CreateAndSetException(exception, value);
#endif // GraalPy change
}

void
PyErr_SetObject(PyObject *exception, PyObject *value)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetObject(tstate, exception, value);
}

#if 0 // GraalPy change
/* Set a key error with the specified argument, wrapping it in a
 * tuple automatically so that tuple keys are not unpacked as the
 * exception arguments. */
void
_PyErr_SetKeyError(PyObject *arg)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *tup = PyTuple_Pack(1, arg);
    if (!tup) {
        /* caller will expect error to be set anyway */
        return;
    }
    _PyErr_SetObject(tstate, PyExc_KeyError, tup);
    Py_DECREF(tup);
}
#endif // GraalPy change

void
_PyErr_SetNone(PyThreadState *tstate, PyObject *exception)
{
    _PyErr_SetObject(tstate, exception, (PyObject *)NULL);
}


void
PyErr_SetNone(PyObject *exception)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetNone(tstate, exception);
}


void
_PyErr_SetString(PyThreadState *tstate, PyObject *exception,
                 const char *string)
{
    PyObject *value = PyUnicode_FromString(string);
    if (value != NULL) {
        _PyErr_SetObject(tstate, exception, value);
        Py_DECREF(value);
    }
}

void
PyErr_SetString(PyObject *exception, const char *string)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetString(tstate, exception, string);
}


PyObject* _Py_HOT_FUNCTION
PyErr_Occurred(void)
{
    /* The caller must hold the GIL. */
    assert(PyGILState_Check());

    PyThreadState *tstate = _PyThreadState_GET();
    return _PyErr_Occurred(tstate);
}


int
PyErr_GivenExceptionMatches(PyObject *err, PyObject *exc)
{
    if (err == NULL || exc == NULL) {
        /* maybe caused by "import exceptions" that failed early on */
        return 0;
    }
    if (PyTuple_Check(exc)) {
        Py_ssize_t i, n;
        n = PyTuple_Size(exc);
        for (i = 0; i < n; i++) {
            /* Test recursively */
             if (PyErr_GivenExceptionMatches(
                 err, PyTuple_GET_ITEM(exc, i)))
             {
                 return 1;
             }
        }
        return 0;
    }
    /* err might be an instance, so check its class. */
    if (PyExceptionInstance_Check(err))
        err = PyExceptionInstance_Class(err);

    if (PyExceptionClass_Check(err) && PyExceptionClass_Check(exc)) {
        return PyType_IsSubtype((PyTypeObject *)err, (PyTypeObject *)exc);
    }

    return err == exc;
}


int
_PyErr_ExceptionMatches(PyThreadState *tstate, PyObject *exc)
{
    return PyErr_GivenExceptionMatches(_PyErr_Occurred(tstate), exc);
}


int
PyErr_ExceptionMatches(PyObject *exc)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyErr_ExceptionMatches(tstate, exc);
}


#ifndef Py_NORMALIZE_RECURSION_LIMIT
#define Py_NORMALIZE_RECURSION_LIMIT 32
#endif

/* Used in many places to normalize a raised exception, including in
   eval_code2(), do_raise(), and PyErr_Print()

   XXX: should PyErr_NormalizeException() also call
            PyException_SetTraceback() with the resulting value and tb?
*/
void
_PyErr_NormalizeException(PyThreadState *tstate, PyObject **exc,
                          PyObject **val, PyObject **tb)
{
    int recursion_depth = 0;
    // GraalPy change: we don't have recursion_headroom
    // tstate->recursion_headroom++;
    PyObject *type, *value, *initial_tb;

  restart:
    type = *exc;
    if (type == NULL) {
        /* There was no exception, so nothing to do. */
        // tstate->recursion_headroom--;
        return;
    }

    value = *val;
    /* If PyErr_SetNone() was used, the value will have been actually
       set to NULL.
    */
    if (!value) {
        value = Py_NewRef(Py_None);
    }

    /* Normalize the exception so that if the type is a class, the
       value will be an instance.
    */
    if (PyExceptionClass_Check(type)) {
        PyObject *inclass = NULL;
        int is_subclass = 0;

        if (PyExceptionInstance_Check(value)) {
            inclass = PyExceptionInstance_Class(value);
            is_subclass = PyObject_IsSubclass(inclass, type);
            if (is_subclass < 0) {
                goto error;
            }
        }

        /* If the value was not an instance, or is not an instance
           whose class is (or is derived from) type, then use the
           value as an argument to instantiation of the type
           class.
        */
        if (!is_subclass) {
            PyObject *fixed_value = _PyErr_CreateException(type, value);
            if (fixed_value == NULL) {
                goto error;
            }
            Py_SETREF(value, fixed_value);
        }
        /* If the class of the instance doesn't exactly match the
           class of the type, believe the instance.
        */
        else if (inclass != type) {
            Py_SETREF(type, Py_NewRef(inclass));
        }
    }
    *exc = type;
    *val = value;
    // tstate->recursion_headroom--;
    return;

  error:
    Py_DECREF(type);
    Py_DECREF(value);
    recursion_depth++;
    if (recursion_depth == Py_NORMALIZE_RECURSION_LIMIT) {
        _PyErr_SetString(tstate, PyExc_RecursionError,
                         "maximum recursion depth exceeded "
                         "while normalizing an exception");
    }
    /* If the new exception doesn't set a traceback and the old
       exception had a traceback, use the old traceback for the
       new exception.  It's better than nothing.
    */
    initial_tb = *tb;
    _PyErr_Fetch(tstate, exc, val, tb);
    assert(*exc != NULL);
    if (initial_tb != NULL) {
        if (*tb == NULL)
            *tb = initial_tb;
        else
            Py_DECREF(initial_tb);
    }
    /* Abort when Py_NORMALIZE_RECURSION_LIMIT has been exceeded, and the
       corresponding RecursionError could not be normalized, and the
       MemoryError raised when normalize this RecursionError could not be
       normalized. */
    if (recursion_depth >= Py_NORMALIZE_RECURSION_LIMIT + 2) {
        if (PyErr_GivenExceptionMatches(*exc, PyExc_MemoryError)) {
            Py_FatalError("Cannot recover from MemoryErrors "
                          "while normalizing exceptions.");
        }
        else {
            Py_FatalError("Cannot recover from the recursive normalization "
                          "of an exception.");
        }
    }
    goto restart;
}


void
PyErr_NormalizeException(PyObject **exc, PyObject **val, PyObject **tb)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_NormalizeException(tstate, exc, val, tb);
}


PyObject *
_PyErr_GetRaisedException(PyThreadState *tstate) {
    PyObject *exc = tstate->current_exception;
    tstate->current_exception = NULL;
    return exc;
}

PyObject *
PyErr_GetRaisedException(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyErr_GetRaisedException(tstate);
}

void
_PyErr_Fetch(PyThreadState *tstate, PyObject **p_type, PyObject **p_value,
             PyObject **p_traceback)
{
    PyObject *exc = _PyErr_GetRaisedException(tstate);
    *p_value = exc;
    if (exc == NULL) {
        *p_type = NULL;
        *p_traceback = NULL;
    }
    else {
        *p_type = Py_NewRef(Py_TYPE(exc));
        // GraalPy change: upcall to get the traceback
        *p_traceback = PyException_GetTraceback(*p_value);
    }
}


void
PyErr_Fetch(PyObject **p_type, PyObject **p_value, PyObject **p_traceback)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_Fetch(tstate, p_type, p_value, p_traceback);
}


void
_PyErr_Clear(PyThreadState *tstate)
{
    _PyErr_Restore(tstate, NULL, NULL, NULL);
}


void
PyErr_Clear(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_Clear(tstate);
}

#if 0 // GraalPy change
static PyObject*
get_exc_type(PyObject *exc_value)  /* returns a borrowed ref */
{
    if (exc_value == NULL || exc_value == Py_None) {
        return Py_None;
    }
    else {
        assert(PyExceptionInstance_Check(exc_value));
        PyObject *type = PyExceptionInstance_Class(exc_value);
        assert(type != NULL);
        return type;
    }
}

static PyObject*
get_exc_traceback(PyObject *exc_value)  /* returns a borrowed ref */
{
    if (exc_value == NULL || exc_value == Py_None) {
        return Py_None;
    }
    else {
        assert(PyExceptionInstance_Check(exc_value));
        PyObject *tb = PyException_GetTraceback(exc_value);
        Py_XDECREF(tb);
        return tb ? tb : Py_None;
    }
}
#endif // GraalPy change

void
_PyErr_GetExcInfo(PyThreadState *tstate,
                  PyObject **p_type, PyObject **p_value, PyObject **p_traceback)
{
    // GraalPy change: different implementation
    PyObject* result = GraalPyTruffleErr_GetExcInfo();
    if(result == NULL) {
        *p_type = Py_NewRef(Py_None);
        *p_value = Py_NewRef(Py_None);
        *p_traceback = Py_NewRef(Py_None);
    } else {
        *p_type = Py_XNewRef(PyTuple_GetItem(result, 0));
        *p_value = Py_XNewRef(PyTuple_GetItem(result, 1));
        *p_traceback = Py_XNewRef(PyTuple_GetItem(result, 2));
        Py_DecRef(result);
    }
}

#if 0 // GraalPy change
PyObject*
_PyErr_GetHandledException(PyThreadState *tstate)
{
    _PyErr_StackItem *exc_info = _PyErr_GetTopmostException(tstate);
    PyObject *exc = exc_info->exc_value;
    if (exc == NULL || exc == Py_None) {
        return NULL;
    }
    return Py_NewRef(exc);
}
#endif // GraalPy change

PyObject*
PyErr_GetHandledException(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyErr_GetHandledException(tstate);
}

#if 0 // GraalPy change
void
_PyErr_SetHandledException(PyThreadState *tstate, PyObject *exc)
{
    Py_XSETREF(tstate->exc_info->exc_value, Py_XNewRef(exc));
}
#endif // GraalPy change

void
PyErr_SetHandledException(PyObject *exc)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetHandledException(tstate, exc);
}

void
PyErr_GetExcInfo(PyObject **p_type, PyObject **p_value, PyObject **p_traceback)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_GetExcInfo(tstate, p_type, p_value, p_traceback);
}

void
PyErr_SetExcInfo(PyObject *type, PyObject *value, PyObject *traceback)
{
    PyErr_SetHandledException(value);
    Py_XDECREF(value);
    /* These args are no longer used, but we still need to steal a ref */
    Py_XDECREF(type);
    Py_XDECREF(traceback);
}


#if 0 // GraalPy change
PyObject*
_PyErr_StackItemToExcInfoTuple(_PyErr_StackItem *err_info)
{
    PyObject *exc_value = err_info->exc_value;

    assert(exc_value == NULL ||
           exc_value == Py_None ||
           PyExceptionInstance_Check(exc_value));

    PyObject *exc_type = get_exc_type(exc_value);
    PyObject *exc_traceback = get_exc_traceback(exc_value);

    return Py_BuildValue(
        "(OOO)",
        exc_type ? exc_type : Py_None,
        exc_value ? exc_value : Py_None,
        exc_traceback ? exc_traceback : Py_None);
}
#endif // GraalPy change


/* Like PyErr_Restore(), but if an exception is already set,
   set the context associated with it.

   The caller is responsible for ensuring that this call won't create
   any cycles in the exception context chain. */
void
_PyErr_ChainExceptions(PyObject *typ, PyObject *val, PyObject *tb)
{
    if (typ == NULL)
        return;

    PyThreadState *tstate = _PyThreadState_GET();

    if (!PyExceptionClass_Check(typ)) {
        _PyErr_Format(tstate, PyExc_SystemError,
                      "_PyErr_ChainExceptions: "
                      "exception %R is not a BaseException subclass",
                      typ);
        return;
    }

    if (_PyErr_Occurred(tstate)) {
        _PyErr_NormalizeException(tstate, &typ, &val, &tb);
        if (tb != NULL) {
            PyException_SetTraceback(val, tb);
            Py_DECREF(tb);
        }
        Py_DECREF(typ);
        PyObject *exc2 = _PyErr_GetRaisedException(tstate);
        PyException_SetContext(exc2, val);
        _PyErr_SetRaisedException(tstate, exc2);
    }
    else {
        _PyErr_Restore(tstate, typ, val, tb);
    }
}

/* Like PyErr_SetRaisedException(), but if an exception is already set,
   set the context associated with it.

   The caller is responsible for ensuring that this call won't create
   any cycles in the exception context chain. */
void
_PyErr_ChainExceptions1(PyObject *exc)
{
    if (exc == NULL) {
        return;
    }
    PyThreadState *tstate = _PyThreadState_GET();
    if (_PyErr_Occurred(tstate)) {
        PyObject *exc2 = _PyErr_GetRaisedException(tstate);
        PyException_SetContext(exc2, exc);
        _PyErr_SetRaisedException(tstate, exc2);
    }
    else {
        _PyErr_SetRaisedException(tstate, exc);
    }
}

#if 0 // GraalPy change
/* Set the currently set exception's context to the given exception.

   If the provided exc_info is NULL, then the current Python thread state's
   exc_info will be used for the context instead.

   This function can only be called when _PyErr_Occurred() is true.
   Also, this function won't create any cycles in the exception context
   chain to the extent that _PyErr_SetObject ensures this. */
void
_PyErr_ChainStackItem(_PyErr_StackItem *exc_info)
{
    PyThreadState *tstate = _PyThreadState_GET();
    assert(_PyErr_Occurred(tstate));

    int exc_info_given;
    if (exc_info == NULL) {
        exc_info_given = 0;
        exc_info = tstate->exc_info;
    } else {
        exc_info_given = 1;
    }

    if (exc_info->exc_value == NULL || exc_info->exc_value == Py_None) {
        return;
    }

    _PyErr_StackItem *saved_exc_info;
    if (exc_info_given) {
        /* Temporarily set the thread state's exc_info since this is what
           _PyErr_SetObject uses for implicit exception chaining. */
        saved_exc_info = tstate->exc_info;
        tstate->exc_info = exc_info;
    }

    PyObject *typ, *val, *tb;
    _PyErr_Fetch(tstate, &typ, &val, &tb);

    /* _PyErr_SetObject sets the context from PyThreadState. */
    _PyErr_SetObject(tstate, typ, val);
    Py_DECREF(typ);  // since _PyErr_Occurred was true
    Py_XDECREF(val);
    Py_XDECREF(tb);

    if (exc_info_given) {
        tstate->exc_info = saved_exc_info;
    }
}
#endif // GraalPy change

static PyObject *
_PyErr_FormatVFromCause(PyThreadState *tstate, PyObject *exception,
                        const char *format, va_list vargs)
{
    assert(_PyErr_Occurred(tstate));
    PyObject *exc = _PyErr_GetRaisedException(tstate);
    assert(!_PyErr_Occurred(tstate));
    _PyErr_FormatV(tstate, exception, format, vargs);
    PyObject *exc2 = _PyErr_GetRaisedException(tstate);
    PyException_SetCause(exc2, Py_NewRef(exc));
    PyException_SetContext(exc2, Py_NewRef(exc));
    Py_DECREF(exc);
    _PyErr_SetRaisedException(tstate, exc2);
    return NULL;
}

NO_INLINE // GraalPy change: disallow bitcode inlining
PyObject *
_PyErr_FormatFromCauseTstate(PyThreadState *tstate, PyObject *exception,
                             const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatVFromCause(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}

NO_INLINE // GraalPy change: disallow bitcode inlining
PyObject *
_PyErr_FormatFromCause(PyObject *exception, const char *format, ...)
{
    PyThreadState *tstate = _PyThreadState_GET();
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatVFromCause(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}

/* Convenience functions to set a type error exception and return 0 */

int
PyErr_BadArgument(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetString(tstate, PyExc_TypeError,
                     "bad argument type for built-in operation");
    return 0;
}

PyObject *
_PyErr_NoMemory(PyThreadState *tstate)
{
    _PyErr_SetNone(tstate, PyExc_MemoryError);
    return NULL;
}

PyObject *
PyErr_NoMemory(void)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyErr_NoMemory(tstate);
}

PyObject *
PyErr_SetFromErrnoWithFilenameObject(PyObject *exc, PyObject *filenameObject)
{
    return PyErr_SetFromErrnoWithFilenameObjects(exc, filenameObject, NULL);
}

PyObject *
PyErr_SetFromErrnoWithFilenameObjects(PyObject *exc, PyObject *filenameObject, PyObject *filenameObject2)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *message;
    PyObject *v, *args;
    int i = errno;
    // GraalPy TODO: missing the win32 bits
    // GraalPy TODO: missing EINTR check

    if (i != 0) {
        char *s = strerror(i);
        // TODO(fa): use PyUnicode_DecodeLocale once available
        // message = PyUnicode_DecodeLocale(s, "surrogateescape");
        message = PyUnicode_FromString(s);
    }
    else {
        /* Sometimes errno didn't get set */
        message = PyUnicode_FromString("Error");
    }

    if (message == NULL)
    {
        return NULL;
    }

    if (filenameObject != NULL) {
        if (filenameObject2 != NULL)
            args = Py_BuildValue("(iOOiO)", i, message, filenameObject, 0, filenameObject2);
        else
            args = Py_BuildValue("(iOO)", i, message, filenameObject);
    } else {
        assert(filenameObject2 == NULL);
        args = Py_BuildValue("(iO)", i, message);
    }
    Py_DECREF(message);

    if (args != NULL) {
        v = PyObject_Call(exc, args, NULL);
        Py_DECREF(args);
        if (v != NULL) {
            _PyErr_SetObject(tstate, (PyObject *) Py_TYPE(v), v);
            Py_DECREF(v);
        }
    }
    return NULL;
}

PyObject *
PyErr_SetFromErrnoWithFilename(PyObject *exc, const char *filename)
{
    PyObject *name = NULL;
    if (filename) {
        int i = errno;
        name = PyUnicode_DecodeFSDefault(filename);
        if (name == NULL) {
            return NULL;
        }
        errno = i;
    }
    PyObject *result = PyErr_SetFromErrnoWithFilenameObjects(exc, name, NULL);
    Py_XDECREF(name);
    return result;
}

PyObject *
PyErr_SetFromErrno(PyObject *exc)
{
    return PyErr_SetFromErrnoWithFilenameObjects(exc, NULL, NULL);
}

#ifdef MS_WINDOWS
/* Windows specific error code handling */
PyObject *PyErr_SetExcFromWindowsErrWithFilenameObject(
    PyObject *exc,
    int ierr,
    PyObject *filenameObject)
{
    return PyErr_SetExcFromWindowsErrWithFilenameObjects(exc, ierr,
        filenameObject, NULL);
}

PyObject *PyErr_SetExcFromWindowsErrWithFilenameObjects(
    PyObject *exc,
    int ierr,
    PyObject *filenameObject,
    PyObject *filenameObject2)
{
    PyThreadState *tstate = _PyThreadState_GET();
    int len;
    WCHAR *s_buf = NULL; /* Free via LocalFree */
    PyObject *message;
    PyObject *args, *v;

    DWORD err = (DWORD)ierr;
    if (err==0) {
        err = GetLastError();
    }

    len = FormatMessageW(
        /* Error API error */
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,           /* no message source */
        err,
        MAKELANGID(LANG_NEUTRAL,
        SUBLANG_DEFAULT), /* Default language */
        (LPWSTR) &s_buf,
        0,              /* size not used */
        NULL);          /* no args */
    if (len==0) {
        /* Only seen this in out of mem situations */
        message = PyUnicode_FromFormat("Windows Error 0x%x", err);
        s_buf = NULL;
    } else {
        /* remove trailing cr/lf and dots */
        while (len > 0 && (s_buf[len-1] <= L' ' || s_buf[len-1] == L'.'))
            s_buf[--len] = L'\0';
        message = PyUnicode_FromWideChar(s_buf, len);
    }

    if (message == NULL)
    {
        LocalFree(s_buf);
        return NULL;
    }

    if (filenameObject == NULL) {
        assert(filenameObject2 == NULL);
        filenameObject = filenameObject2 = Py_None;
    }
    else if (filenameObject2 == NULL)
        filenameObject2 = Py_None;
    /* This is the constructor signature for OSError.
       The POSIX translation will be figured out by the constructor. */
    args = Py_BuildValue("(iOOiO)", 0, message, filenameObject, err, filenameObject2);
    Py_DECREF(message);

    if (args != NULL) {
        v = PyObject_Call(exc, args, NULL);
        Py_DECREF(args);
        if (v != NULL) {
            _PyErr_SetObject(tstate, (PyObject *) Py_TYPE(v), v);
            Py_DECREF(v);
        }
    }
    LocalFree(s_buf);
    return NULL;
}

PyObject *PyErr_SetExcFromWindowsErrWithFilename(
    PyObject *exc,
    int ierr,
    const char *filename)
{
    PyObject *name = NULL;
    if (filename) {
        if ((DWORD)ierr == 0) {
            ierr = (int)GetLastError();
        }
        name = PyUnicode_DecodeFSDefault(filename);
        if (name == NULL) {
            return NULL;
        }
    }
    PyObject *ret = PyErr_SetExcFromWindowsErrWithFilenameObjects(exc,
                                                                 ierr,
                                                                 name,
                                                                 NULL);
    Py_XDECREF(name);
    return ret;
}

PyObject *PyErr_SetExcFromWindowsErr(PyObject *exc, int ierr)
{
    return PyErr_SetExcFromWindowsErrWithFilename(exc, ierr, NULL);
}

PyObject *PyErr_SetFromWindowsErr(int ierr)
{
    return PyErr_SetExcFromWindowsErrWithFilename(PyExc_OSError,
                                                  ierr, NULL);
}

PyObject *PyErr_SetFromWindowsErrWithFilename(
    int ierr,
    const char *filename)
{
    PyObject *name = NULL;
    if (filename) {
        if ((DWORD)ierr == 0) {
            ierr = (int)GetLastError();
        }
        name = PyUnicode_DecodeFSDefault(filename);
        if (name == NULL) {
            return NULL;
        }
    }
    PyObject *result = PyErr_SetExcFromWindowsErrWithFilenameObjects(
                                                  PyExc_OSError,
                                                  ierr, name, NULL);
    Py_XDECREF(name);
    return result;
}

#endif /* MS_WINDOWS */

#if 0 // GraalPy change
static PyObject *
_PyErr_SetImportErrorSubclassWithNameFrom(
    PyObject *exception, PyObject *msg,
    PyObject *name, PyObject *path, PyObject* from_name)
{
    PyThreadState *tstate = _PyThreadState_GET();
    int issubclass;
    PyObject *kwargs, *error;

    issubclass = PyObject_IsSubclass(exception, PyExc_ImportError);
    if (issubclass < 0) {
        return NULL;
    }
    else if (!issubclass) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "expected a subclass of ImportError");
        return NULL;
    }

    if (msg == NULL) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "expected a message argument");
        return NULL;
    }

    if (name == NULL) {
        name = Py_None;
    }
    if (path == NULL) {
        path = Py_None;
    }
    if (from_name == NULL) {
        from_name = Py_None;
    }


    kwargs = PyDict_New();
    if (kwargs == NULL) {
        return NULL;
    }
    if (PyDict_SetItemString(kwargs, "name", name) < 0) {
        goto done;
    }
    if (PyDict_SetItemString(kwargs, "path", path) < 0) {
        goto done;
    }
    if (PyDict_SetItemString(kwargs, "name_from", from_name) < 0) {
        goto done;
    }

    error = PyObject_VectorcallDict(exception, &msg, 1, kwargs);
    if (error != NULL) {
        _PyErr_SetObject(tstate, (PyObject *)Py_TYPE(error), error);
        Py_DECREF(error);
    }

done:
    Py_DECREF(kwargs);
    return NULL;
}


PyObject *
PyErr_SetImportErrorSubclass(PyObject *exception, PyObject *msg,
    PyObject *name, PyObject *path)
{
    return _PyErr_SetImportErrorSubclassWithNameFrom(exception, msg, name, path, NULL);
}

PyObject *
_PyErr_SetImportErrorWithNameFrom(PyObject *msg, PyObject *name, PyObject *path, PyObject* from_name)
{
    return _PyErr_SetImportErrorSubclassWithNameFrom(PyExc_ImportError, msg, name, path, from_name);
}

PyObject *
PyErr_SetImportError(PyObject *msg, PyObject *name, PyObject *path)
{
    return PyErr_SetImportErrorSubclass(PyExc_ImportError, msg, name, path);
}
#endif // GraalPy change

void
_PyErr_BadInternalCall(const char *filename, int lineno)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_Format(tstate, PyExc_SystemError,
                  "%s:%d: bad argument to internal function",
                  filename, lineno);
}

/* Remove the preprocessor macro for PyErr_BadInternalCall() so that we can
   export the entry point for existing object code: */
#undef PyErr_BadInternalCall
void
PyErr_BadInternalCall(void)
{
    assert(0 && "bad argument to internal function");
    PyThreadState *tstate = _PyThreadState_GET();
    _PyErr_SetString(tstate, PyExc_SystemError,
                     "bad argument to internal function");
}
#define PyErr_BadInternalCall() _PyErr_BadInternalCall(__FILE__, __LINE__)


static PyObject *
_PyErr_FormatV(PyThreadState *tstate, PyObject *exception,
               const char *format, va_list vargs)
{
    PyObject* string;

    /* Issue #23571: PyUnicode_FromFormatV() must not be called with an
       exception set, it calls arbitrary Python code like PyObject_Repr() */
    _PyErr_Clear(tstate);

    string = PyUnicode_FromFormatV(format, vargs);
    if (string != NULL) {
        _PyErr_SetObject(tstate, exception, string);
        Py_DECREF(string);
    }
    return NULL;
}


PyObject *
PyErr_FormatV(PyObject *exception, const char *format, va_list vargs)
{
    PyThreadState *tstate = _PyThreadState_GET();
    return _PyErr_FormatV(tstate, exception, format, vargs);
}


NO_INLINE // GraalPy change: disallow bitcode inlining
PyObject *
_PyErr_Format(PyThreadState *tstate, PyObject *exception,
              const char *format, ...)
{
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatV(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}


NO_INLINE // GraalPy change: disallow bitcode inlining
PyObject *
PyErr_Format(PyObject *exception, const char *format, ...)
{
    PyThreadState *tstate = _PyThreadState_GET();
    va_list vargs;
    va_start(vargs, format);
    _PyErr_FormatV(tstate, exception, format, vargs);
    va_end(vargs);
    return NULL;
}


#if 0 // GraalPy change
/* Adds a note to the current exception (if any) */
void
_PyErr_FormatNote(const char *format, ...)
{
    PyObject *exc = PyErr_GetRaisedException();
    if (exc == NULL) {
        return;
    }
    va_list vargs;
    va_start(vargs, format);
    PyObject *note = PyUnicode_FromFormatV(format, vargs);
    va_end(vargs);
    if (note == NULL) {
        goto error;
    }
    int res = _PyException_AddNote(exc, note);
    Py_DECREF(note);
    if (res < 0) {
        goto error;
    }
    PyErr_SetRaisedException(exc);
    return;
error:
    _PyErr_ChainExceptions1(exc);
}


PyObject *
PyErr_NewException(const char *name, PyObject *base, PyObject *dict)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *modulename = NULL;
    PyObject *mydict = NULL;
    PyObject *bases = NULL;
    PyObject *result = NULL;

    const char *dot = strrchr(name, '.');
    if (dot == NULL) {
        _PyErr_SetString(tstate, PyExc_SystemError,
                         "PyErr_NewException: name must be module.class");
        return NULL;
    }
    if (base == NULL) {
        base = PyExc_Exception;
    }
    if (dict == NULL) {
        dict = mydict = PyDict_New();
        if (dict == NULL)
            goto failure;
    }

    int r = PyDict_Contains(dict, &_Py_ID(__module__));
    if (r < 0) {
        goto failure;
    }
    if (r == 0) {
        modulename = PyUnicode_FromStringAndSize(name,
                                             (Py_ssize_t)(dot-name));
        if (modulename == NULL)
            goto failure;
        if (PyDict_SetItem(dict, &_Py_ID(__module__), modulename) != 0)
            goto failure;
    }
    if (PyTuple_Check(base)) {
        bases = Py_NewRef(base);
    } else {
        bases = PyTuple_Pack(1, base);
        if (bases == NULL)
            goto failure;
    }
    /* Create a real class. */
    result = PyObject_CallFunction((PyObject *)&PyType_Type, "sOO",
                                   dot+1, bases, dict);
  failure:
    Py_XDECREF(bases);
    Py_XDECREF(mydict);
    Py_XDECREF(modulename);
    return result;
}


/* Create an exception with docstring */
PyObject *
PyErr_NewExceptionWithDoc(const char *name, const char *doc,
                          PyObject *base, PyObject *dict)
{
    int result;
    PyObject *ret = NULL;
    PyObject *mydict = NULL; /* points to the dict only if we create it */
    PyObject *docobj;

    if (dict == NULL) {
        dict = mydict = PyDict_New();
        if (dict == NULL) {
            return NULL;
        }
    }

    if (doc != NULL) {
        docobj = PyUnicode_FromString(doc);
        if (docobj == NULL)
            goto failure;
        result = PyDict_SetItemString(dict, "__doc__", docobj);
        Py_DECREF(docobj);
        if (result < 0)
            goto failure;
    }

    ret = PyErr_NewException(name, base, dict);
  failure:
    Py_XDECREF(mydict);
    return ret;
}


PyDoc_STRVAR(UnraisableHookArgs__doc__,
"UnraisableHookArgs\n\
\n\
Type used to pass arguments to sys.unraisablehook.");

static PyTypeObject UnraisableHookArgsType;

static PyStructSequence_Field UnraisableHookArgs_fields[] = {
    {"exc_type", "Exception type"},
    {"exc_value", "Exception value"},
    {"exc_traceback", "Exception traceback"},
    {"err_msg", "Error message"},
    {"object", "Object causing the exception"},
    {0}
};

static PyStructSequence_Desc UnraisableHookArgs_desc = {
    .name = "UnraisableHookArgs",
    .doc = UnraisableHookArgs__doc__,
    .fields = UnraisableHookArgs_fields,
    .n_in_sequence = 5
};


PyStatus
_PyErr_InitTypes(PyInterpreterState *interp)
{
    if (_PyStructSequence_InitBuiltin(interp, &UnraisableHookArgsType,
                                      &UnraisableHookArgs_desc) < 0)
    {
        return _PyStatus_ERR("failed to initialize UnraisableHookArgs type");
    }
    return _PyStatus_OK();
}


void
_PyErr_FiniTypes(PyInterpreterState *interp)
{
    _PyStructSequence_FiniBuiltin(interp, &UnraisableHookArgsType);
}


static PyObject *
make_unraisable_hook_args(PyThreadState *tstate, PyObject *exc_type,
                          PyObject *exc_value, PyObject *exc_tb,
                          PyObject *err_msg, PyObject *obj)
{
    PyObject *args = PyStructSequence_New(&UnraisableHookArgsType);
    if (args == NULL) {
        return NULL;
    }

    Py_ssize_t pos = 0;
#define ADD_ITEM(exc_type) \
        do { \
            if (exc_type == NULL) { \
                exc_type = Py_None; \
            } \
            PyStructSequence_SET_ITEM(args, pos++, Py_NewRef(exc_type)); \
        } while (0)


    ADD_ITEM(exc_type);
    ADD_ITEM(exc_value);
    ADD_ITEM(exc_tb);
    ADD_ITEM(err_msg);
    ADD_ITEM(obj);
#undef ADD_ITEM

    if (_PyErr_Occurred(tstate)) {
        Py_DECREF(args);
        return NULL;
    }
    return args;
}



/* Default implementation of sys.unraisablehook.

   It can be called to log the exception of a custom sys.unraisablehook.

   Do nothing if sys.stderr attribute doesn't exist or is set to None. */
static int
write_unraisable_exc_file(PyThreadState *tstate, PyObject *exc_type,
                          PyObject *exc_value, PyObject *exc_tb,
                          PyObject *err_msg, PyObject *obj, PyObject *file)
{
    if (obj != NULL && obj != Py_None) {
        if (err_msg != NULL && err_msg != Py_None) {
            if (PyFile_WriteObject(err_msg, file, Py_PRINT_RAW) < 0) {
                return -1;
            }
            if (PyFile_WriteString(": ", file) < 0) {
                return -1;
            }
        }
        else {
            if (PyFile_WriteString("Exception ignored in: ", file) < 0) {
                return -1;
            }
        }

        if (PyFile_WriteObject(obj, file, 0) < 0) {
            _PyErr_Clear(tstate);
            if (PyFile_WriteString("<object repr() failed>", file) < 0) {
                return -1;
            }
        }
        if (PyFile_WriteString("\n", file) < 0) {
            return -1;
        }
    }
    else if (err_msg != NULL && err_msg != Py_None) {
        if (PyFile_WriteObject(err_msg, file, Py_PRINT_RAW) < 0) {
            return -1;
        }
        if (PyFile_WriteString(":\n", file) < 0) {
            return -1;
        }
    }

    if (exc_tb != NULL && exc_tb != Py_None) {
        if (PyTraceBack_Print(exc_tb, file) < 0) {
            /* continue even if writing the traceback failed */
            _PyErr_Clear(tstate);
        }
    }

    if (exc_type == NULL || exc_type == Py_None) {
        return -1;
    }

    assert(PyExceptionClass_Check(exc_type));

    PyObject *modulename = PyObject_GetAttr(exc_type, &_Py_ID(__module__));
    if (modulename == NULL || !PyUnicode_Check(modulename)) {
        Py_XDECREF(modulename);
        _PyErr_Clear(tstate);
        if (PyFile_WriteString("<unknown>", file) < 0) {
            return -1;
        }
    }
    else {
        if (!_PyUnicode_Equal(modulename, &_Py_ID(builtins)) &&
            !_PyUnicode_Equal(modulename, &_Py_ID(__main__))) {
            if (PyFile_WriteObject(modulename, file, Py_PRINT_RAW) < 0) {
                Py_DECREF(modulename);
                return -1;
            }
            Py_DECREF(modulename);
            if (PyFile_WriteString(".", file) < 0) {
                return -1;
            }
        }
        else {
            Py_DECREF(modulename);
        }
    }

    PyObject *qualname = PyType_GetQualName((PyTypeObject *)exc_type);
    if (qualname == NULL || !PyUnicode_Check(qualname)) {
        Py_XDECREF(qualname);
        _PyErr_Clear(tstate);
        if (PyFile_WriteString("<unknown>", file) < 0) {
            return -1;
        }
    }
    else {
        if (PyFile_WriteObject(qualname, file, Py_PRINT_RAW) < 0) {
            Py_DECREF(qualname);
            return -1;
        }
        Py_DECREF(qualname);
    }

    if (exc_value && exc_value != Py_None) {
        if (PyFile_WriteString(": ", file) < 0) {
            return -1;
        }
        if (PyFile_WriteObject(exc_value, file, Py_PRINT_RAW) < 0) {
            _PyErr_Clear(tstate);
            if (PyFile_WriteString("<exception str() failed>", file) < 0) {
                return -1;
            }
        }
    }

    if (PyFile_WriteString("\n", file) < 0) {
        return -1;
    }

    /* Explicitly call file.flush() */
    PyObject *res = _PyObject_CallMethodNoArgs(file, &_Py_ID(flush));
    if (!res) {
        return -1;
    }
    Py_DECREF(res);

    return 0;
}


static int
write_unraisable_exc(PyThreadState *tstate, PyObject *exc_type,
                     PyObject *exc_value, PyObject *exc_tb, PyObject *err_msg,
                     PyObject *obj)
{
    PyObject *file = _PySys_GetAttr(tstate, &_Py_ID(stderr));
    if (file == NULL || file == Py_None) {
        return 0;
    }

    /* Hold a strong reference to ensure that sys.stderr doesn't go away
       while we use it */
    Py_INCREF(file);
    int res = write_unraisable_exc_file(tstate, exc_type, exc_value, exc_tb,
                                        err_msg, obj, file);
    Py_DECREF(file);

    return res;
}


PyObject*
_PyErr_WriteUnraisableDefaultHook(PyObject *args)
{
    PyThreadState *tstate = _PyThreadState_GET();

    if (!Py_IS_TYPE(args, &UnraisableHookArgsType)) {
        _PyErr_SetString(tstate, PyExc_TypeError,
                         "sys.unraisablehook argument type "
                         "must be UnraisableHookArgs");
        return NULL;
    }

    /* Borrowed references */
    PyObject *exc_type = PyStructSequence_GET_ITEM(args, 0);
    PyObject *exc_value = PyStructSequence_GET_ITEM(args, 1);
    PyObject *exc_tb = PyStructSequence_GET_ITEM(args, 2);
    PyObject *err_msg = PyStructSequence_GET_ITEM(args, 3);
    PyObject *obj = PyStructSequence_GET_ITEM(args, 4);

    if (write_unraisable_exc(tstate, exc_type, exc_value, exc_tb, err_msg, obj) < 0) {
        return NULL;
    }
    Py_RETURN_NONE;
}


/* Call sys.unraisablehook().

   This function can be used when an exception has occurred but there is no way
   for Python to handle it. For example, when a destructor raises an exception
   or during garbage collection (gc.collect()).

   If err_msg_str is non-NULL, the error message is formatted as:
   "Exception ignored %s" % err_msg_str. Otherwise, use "Exception ignored in"
   error message.

   An exception must be set when calling this function. */
void
_PyErr_WriteUnraisableMsg(const char *err_msg_str, PyObject *obj)
{
    PyThreadState *tstate = _PyThreadState_GET();
    _Py_EnsureTstateNotNULL(tstate);

    PyObject *err_msg = NULL;
    PyObject *exc_type, *exc_value, *exc_tb;
    _PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);

    assert(exc_type != NULL);

    if (exc_type == NULL) {
        /* sys.unraisablehook requires that at least exc_type is set */
        goto default_hook;
    }

    if (exc_tb == NULL) {
        PyFrameObject *frame = PyThreadState_GetFrame(tstate);
        if (frame != NULL) {
            exc_tb = _PyTraceBack_FromFrame(NULL, frame);
            if (exc_tb == NULL) {
                _PyErr_Clear(tstate);
            }
            Py_DECREF(frame);
        }
    }

    _PyErr_NormalizeException(tstate, &exc_type, &exc_value, &exc_tb);

    if (exc_tb != NULL && exc_tb != Py_None && PyTraceBack_Check(exc_tb)) {
        if (PyException_SetTraceback(exc_value, exc_tb) < 0) {
            _PyErr_Clear(tstate);
        }
    }

    if (err_msg_str != NULL) {
        err_msg = PyUnicode_FromFormat("Exception ignored %s", err_msg_str);
        if (err_msg == NULL) {
            PyErr_Clear();
        }
    }

    PyObject *hook_args = make_unraisable_hook_args(
        tstate, exc_type, exc_value, exc_tb, err_msg, obj);
    if (hook_args == NULL) {
        err_msg_str = ("Exception ignored on building "
                       "sys.unraisablehook arguments");
        goto error;
    }

    PyObject *hook = _PySys_GetAttr(tstate, &_Py_ID(unraisablehook));
    if (hook == NULL) {
        Py_DECREF(hook_args);
        goto default_hook;
    }

    if (_PySys_Audit(tstate, "sys.unraisablehook", "OO", hook, hook_args) < 0) {
        Py_DECREF(hook_args);
        err_msg_str = "Exception ignored in audit hook";
        obj = NULL;
        goto error;
    }

    if (hook == Py_None) {
        Py_DECREF(hook_args);
        goto default_hook;
    }

    PyObject *res = PyObject_CallOneArg(hook, hook_args);
    Py_DECREF(hook_args);
    if (res != NULL) {
        Py_DECREF(res);
        goto done;
    }

    /* sys.unraisablehook failed: log its error using default hook */
    obj = hook;
    err_msg_str = NULL;

error:
    /* err_msg_str and obj have been updated and we have a new exception */
    Py_XSETREF(err_msg, PyUnicode_FromString(err_msg_str ?
        err_msg_str : "Exception ignored in sys.unraisablehook"));
    Py_XDECREF(exc_type);
    Py_XDECREF(exc_value);
    Py_XDECREF(exc_tb);
    _PyErr_Fetch(tstate, &exc_type, &exc_value, &exc_tb);

default_hook:
    /* Call the default unraisable hook (ignore failure) */
    (void)write_unraisable_exc(tstate, exc_type, exc_value, exc_tb,
                               err_msg, obj);

done:
    Py_XDECREF(exc_type);
    Py_XDECREF(exc_value);
    Py_XDECREF(exc_tb);
    Py_XDECREF(err_msg);
    _PyErr_Clear(tstate); /* Just in case */
}
#endif // GraalPy change


void
PyErr_WriteUnraisable(PyObject *obj)
{
    _PyErr_WriteUnraisableMsg(NULL, obj);
}


#if 0 // GraalPy change
void
PyErr_SyntaxLocation(const char *filename, int lineno)
{
    PyErr_SyntaxLocationEx(filename, lineno, -1);
}


/* Set file and line information for the current exception.
   If the exception is not a SyntaxError, also sets additional attributes
   to make printing of exceptions believe it is a syntax error. */

static void
PyErr_SyntaxLocationObjectEx(PyObject *filename, int lineno, int col_offset,
                             int end_lineno, int end_col_offset)
{
    PyThreadState *tstate = _PyThreadState_GET();

    /* add attributes for the line number and filename for the error */
    PyObject *exc = _PyErr_GetRaisedException(tstate);
    /* XXX check that it is, indeed, a syntax error. It might not
     * be, though. */
    PyObject *tmp = PyLong_FromLong(lineno);
    if (tmp == NULL) {
        _PyErr_Clear(tstate);
    }
    else {
        if (PyObject_SetAttr(exc, &_Py_ID(lineno), tmp)) {
            _PyErr_Clear(tstate);
        }
        Py_DECREF(tmp);
    }
    tmp = NULL;
    if (col_offset >= 0) {
        tmp = PyLong_FromLong(col_offset);
        if (tmp == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    if (PyObject_SetAttr(exc, &_Py_ID(offset), tmp ? tmp : Py_None)) {
        _PyErr_Clear(tstate);
    }
    Py_XDECREF(tmp);

    tmp = NULL;
    if (end_lineno >= 0) {
        tmp = PyLong_FromLong(end_lineno);
        if (tmp == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    if (PyObject_SetAttr(exc, &_Py_ID(end_lineno), tmp ? tmp : Py_None)) {
        _PyErr_Clear(tstate);
    }
    Py_XDECREF(tmp);

    tmp = NULL;
    if (end_col_offset >= 0) {
        tmp = PyLong_FromLong(end_col_offset);
        if (tmp == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    if (PyObject_SetAttr(exc, &_Py_ID(end_offset), tmp ? tmp : Py_None)) {
        _PyErr_Clear(tstate);
    }
    Py_XDECREF(tmp);

    tmp = NULL;
    if (filename != NULL) {
        if (PyObject_SetAttr(exc, &_Py_ID(filename), filename)) {
            _PyErr_Clear(tstate);
        }

        tmp = PyErr_ProgramTextObject(filename, lineno);
        if (tmp) {
            if (PyObject_SetAttr(exc, &_Py_ID(text), tmp)) {
                _PyErr_Clear(tstate);
            }
            Py_DECREF(tmp);
        }
        else {
            _PyErr_Clear(tstate);
        }
    }
    if ((PyObject *)Py_TYPE(exc) != PyExc_SyntaxError) {
        if (_PyObject_LookupAttr(exc, &_Py_ID(msg), &tmp) < 0) {
            _PyErr_Clear(tstate);
        }
        else if (tmp) {
            Py_DECREF(tmp);
        }
        else {
            tmp = PyObject_Str(exc);
            if (tmp) {
                if (PyObject_SetAttr(exc, &_Py_ID(msg), tmp)) {
                    _PyErr_Clear(tstate);
                }
                Py_DECREF(tmp);
            }
            else {
                _PyErr_Clear(tstate);
            }
        }

        if (_PyObject_LookupAttr(exc, &_Py_ID(print_file_and_line), &tmp) < 0) {
            _PyErr_Clear(tstate);
        }
        else if (tmp) {
            Py_DECREF(tmp);
        }
        else {
            if (PyObject_SetAttr(exc, &_Py_ID(print_file_and_line), Py_None)) {
                _PyErr_Clear(tstate);
            }
        }
    }
    _PyErr_SetRaisedException(tstate, exc);
}

void
PyErr_SyntaxLocationObject(PyObject *filename, int lineno, int col_offset) {
    PyErr_SyntaxLocationObjectEx(filename, lineno, col_offset, lineno, -1);
}

void
PyErr_RangedSyntaxLocationObject(PyObject *filename, int lineno, int col_offset,
                                 int end_lineno, int end_col_offset) {
    PyErr_SyntaxLocationObjectEx(filename, lineno, col_offset, end_lineno, end_col_offset);
}

void
PyErr_SyntaxLocationEx(const char *filename, int lineno, int col_offset)
{
    PyThreadState *tstate = _PyThreadState_GET();
    PyObject *fileobj;
    if (filename != NULL) {
        fileobj = PyUnicode_DecodeFSDefault(filename);
        if (fileobj == NULL) {
            _PyErr_Clear(tstate);
        }
    }
    else {
        fileobj = NULL;
    }
    PyErr_SyntaxLocationObject(fileobj, lineno, col_offset);
    Py_XDECREF(fileobj);
}

/* Attempt to load the line of text that the exception refers to.  If it
   fails, it will return NULL but will not set an exception.

   XXX The functionality of this function is quite similar to the
   functionality in tb_displayline() in traceback.c. */

static PyObject *
err_programtext(FILE *fp, int lineno, const char* encoding)
{
    char linebuf[1000];
    size_t line_size = 0;

    for (int i = 0; i < lineno; ) {
        line_size = 0;
        if (_Py_UniversalNewlineFgetsWithSize(linebuf, sizeof(linebuf),
                                              fp, NULL, &line_size) == NULL)
        {
            /* Error or EOF. */
            return NULL;
        }
        /* fgets read *something*; if it didn't fill the
           whole buffer, it must have found a newline
           or hit the end of the file; if the last character is \n,
           it obviously found a newline; else we haven't
           yet seen a newline, so must continue */
        if (i + 1 < lineno
            && line_size == sizeof(linebuf) - 1
            && linebuf[sizeof(linebuf) - 2] != '\n')
        {
            continue;
        }
        i++;
    }

    const char *line = linebuf;
    /* Skip BOM. */
    if (lineno == 1 && line_size >= 3 && memcmp(line, "\xef\xbb\xbf", 3) == 0) {
        line += 3;
        line_size -= 3;
    }
    PyObject *res = PyUnicode_Decode(line, line_size, encoding, "replace");
    if (res == NULL) {
        PyErr_Clear();
    }
    return res;
}

PyObject *
PyErr_ProgramText(const char *filename, int lineno)
{
    if (filename == NULL) {
        return NULL;
    }

    PyObject *filename_obj = PyUnicode_DecodeFSDefault(filename);
    if (filename_obj == NULL) {
        PyErr_Clear();
        return NULL;
    }
    PyObject *res = PyErr_ProgramTextObject(filename_obj, lineno);
    Py_DECREF(filename_obj);
    return res;
}

/* Function from Parser/tokenizer/file_tokenizer.c */
extern char* _PyTokenizer_FindEncodingFilename(int, PyObject *);

PyObject *
_PyErr_ProgramDecodedTextObject(PyObject *filename, int lineno, const char* encoding)
{
    char *found_encoding = NULL;
    if (filename == NULL || lineno <= 0) {
        return NULL;
    }

    FILE *fp = _Py_fopen_obj(filename, "r" PY_STDIOTEXTMODE);
    if (fp == NULL) {
        PyErr_Clear();
        return NULL;
    }
    if (encoding == NULL) {
        int fd = fileno(fp);
        found_encoding = _PyTokenizer_FindEncodingFilename(fd, filename);
        encoding = found_encoding;
        if (encoding == NULL) {
            PyErr_Clear();
            encoding = "utf-8";
        }
        /* Reset position */
        if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
            fclose(fp);
            PyMem_Free(found_encoding);
            return NULL;
        }
    }
    PyObject *res = err_programtext(fp, lineno, encoding);
    fclose(fp);
    PyMem_Free(found_encoding);
    return res;
}

PyObject *
PyErr_ProgramTextObject(PyObject *filename, int lineno)
{
    return _PyErr_ProgramDecodedTextObject(filename, lineno, NULL);
}
#endif // GraalPy change

#ifdef __cplusplus
}
#endif

/*-*- c++ -*-********************************************************
 * PythonHelp.h - Helper functions for the python interface         *
 *                                                                  *
 * (C) 1998 by Karsten Ball�der (Ballueder@usa.net)                 *
 *                                                                  *
 * $Id$

 *******************************************************************/

#ifndef   PYTHONHELP_H
#   define PYTHONHELP_H

#include   "Profile.h"

/** Function to call a callback function.
    The callback will be called with its name and the object as first arguments.
    @param name name of the callback name in the profiles
    @param obj pointer to the object calling it
    @param class classname of the object
    @param profile (optional) profile to evaluate
    @param argfmt format for additional arguments
    @return an integer value
*/
int
PythonCallback(const char *name, void *obj, const char *classname,
               ProfileBase *profile = NULL, const char *argfmt = NULL,
               ...);

/** Function to create a Python object being a pointer to a C++
    object, e.g. a StringPtr object from a String * in C++.
    @param obj - Pointer to a C++ object
    @param classname - Name of the class
    @return the Python Object
*/

PyObject *PyH_makeObjectFromPointer(void *obj,const char *classname);

/** Call a callback function:
    @param func Name of the python function, possibly Module.name
    @param name  Name of the callback
    @param obj  Pointer to a C++ object initiating the callback
    @param classname  Name of the object's class
    @param resultfmt Format string for the result pointer
    @param result Pointer where to store the result
    @param parg A Python object to pass as additional argument
    @return true on success
*/

bool PyH_CallFunction(const char *func,
                      const char *name,
                      void *obj, const char *classname,
                      const char *resultfmt, void *result,
                      PyObject *parg = NULL);
   
/// the two possible modes for PyH_Run_Codestr():
enum PyH_RunModes { PY_STATEMENT, PY_EXPRESSION };

/** Function to run statements or expressions in python.
    @param mode either PY_STATMENT or PY_EXPRESSION
    @param code the python code to run
    @param modname the module to load (if needed, otherwise __main__)
    @param resfmt format string for the result
    @param cresult pointer where to store the result
    @return 0 on success
*/
int
PyH_RunCodestr(enum PyH_RunModes mode, const char *code,      /* expr or stmt string */
                const char *modname,                     /* loads module if needed */
                const char *resfmt, void *cresult);       /* converts expr result to C */

/** Function to call a python function
    @param funcname the function to call
    @param modname the module to load (if needed, otherwise __main__)
    @param resfmt format string for the result
    @param cresult pointer where to store the result
    @param argfmt argument format string
    @param ... the arguments
    @return 0 on error
*/
int
PyH_RunFunction(const char *funcname, const char *modname,          /* load from module */
                const char *resfmt,  void *cresult,           /* convert to c/c++ */
                const char *argfmt,  ... /* arg, arg... */ );  /* convert to python */

/// macro to run an expression
#define   PyH_Expression(exp,module,resfmt,cresult) \
   PyH_RunCodestr(PY_EXPRESSION,exp,module, resfmt, cresult)

/// macro to run a statement      
#define   PyH_Statement(stat,module,resfmt,cresult) \
   PyH_RunCodestr(PY_STATEMENT,stat,module, resfmt, cresult)

/** Converts a PyObject to a C++ variable in a safe way.
    @param presult the object to convert
    @param resFormat the format string
    @param resTarget where to store it
    @return 0 on error
*/
int PyH_ConvertResult(PyObject *presult, const char *resFormat, void *resTarget);

/** Gets the last error message/traceback from Python and stores it in a String.
    @param errString where to store the error message/traceback
*/
void PyH_GetErrorMessage(String *errString);

#endif

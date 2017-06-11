// -*- c -*-

%init %{
  import_array();
%}
 
%typemap(in) (REAL [], int) (PyArrayObject *arr= NULL) {
   int sm_REAL = (sizeof(REAL) == sizeof(double) ? PyArray_DOUBLE : PyArray_FLOAT);
   
   arr = (PyArrayObject *)PyArray_ContiguousFromObject($input, sm_REAL, 1, 1);

   if (arr == NULL) {
      PyErr_Clear();
      if(PyArray_Check($input)) {
	 arr = (PyArrayObject *)PyArray_Cast((PyArrayObject *)PyArray_Copy((PyArrayObject *)$input), sm_REAL);
      } else if(PyInt_Check($input) || PyFloat_Check($input)) {
	 int dims[1] = {1};
	 arr = (PyArrayObject *)PyArray_FromDims(1, dims, sm_REAL);
	 ((REAL *)(arr->data))[0] = PyInt_Check($input)? PyInt_AsLong($input) : PyFloat_AsDouble($input);
      }
   }
   
   if (arr == NULL) {
      PyObject *ptype; PyObject *pvalue; PyObject *ptraceback;
      char *msg;
      PyErr_Fetch(&ptype, &pvalue, &ptraceback);
      msg = (pvalue == NULL ? "Can't cast array to REAL" : PyString_AsString(PyObject_Str(pvalue)));
      Py_XDECREF(ptype); Py_XDECREF(pvalue); Py_XDECREF(ptraceback);
      SWIG_exception(SWIG_RuntimeError, msg);

      return 0;
   }
   $1 = (REAL *)arr->data;
   $2 = arr->dimensions[0];
}

%typemap(freearg) (REAL [], int) {
   Py_XDECREF(arr$argnum);
}

/***********************************************************************************************************/

%typemap(in) (REAL **, int, int) (PyArrayObject *arr= NULL, REAL **rows = NULL ) {
   int sm_REAL = (sizeof(REAL) == sizeof(double) ? PyArray_DOUBLE : PyArray_FLOAT);
   
   arr = (PyArrayObject *)PyArray_ContiguousFromObject($input, sm_REAL, 2, 2);

   if (arr == NULL) {
      PyErr_Clear();
      if(PyArray_Check($input)) {
	 arr = (PyArrayObject *)PyArray_Cast((PyArrayObject *)PyArray_Copy((PyArrayObject *)$input), sm_REAL);
      } else if(PyInt_Check($input) || PyFloat_Check($input)) {
	 int dims[2] = {1, 1};
	 arr = (PyArrayObject *)PyArray_FromDims(2, dims, sm_REAL);
	 ((REAL *)(arr->data))[0] = PyInt_Check($input)? PyInt_AsLong($input) : PyFloat_AsDouble($input);
      }
   }
   
   if (arr == NULL) {
      PyObject *ptype; PyObject *pvalue; PyObject *ptraceback;
      char *msg;
      PyErr_Fetch(&ptype, &pvalue, &ptraceback);
      msg = (pvalue == NULL ? "Can't cast array to REAL" : PyString_AsString(PyObject_Str(pvalue)));
      Py_XDECREF(ptype); Py_XDECREF(pvalue); Py_XDECREF(ptraceback);
      SWIG_exception(SWIG_RuntimeError, msg);

      return 0;
   }

   if (arr->nd != 2) {
      char msg[] = "Expected a 2-D array; saw a XX-D array" "extra space";
      sprintf(msg, "Expected a 2-D array; saw a %d-D array", arr->nd);
      SWIG_exception(SWIG_RuntimeError, msg);

      return 0;
   }

   $2 = arr->dimensions[1];
   $3 = arr->dimensions[0];

   rows = malloc($3*sizeof(REAL *));
   {
      int i;
      for (i = 0; i < $3; i++) {
	 rows[i] = (REAL *)(arr->data + i*arr->strides[0]);
      }
   }
   $1 = rows;
}

%typemap(freearg) (REAL **, int, int) {
   Py_XDECREF(arr$argnum);
   free(rows$argnum);
}

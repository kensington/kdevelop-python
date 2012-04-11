// avoid compiler warnings... urgh
#undef _POSIX_C_SOURCE
#undef _XOPEN_SOURCE

#include <language/duchain/duchainlock.h>

#include <python-kdevelop/pyport.h>
#include <python-kdevelop/pyconfig.h>
#include <python-kdevelop/node.h>

#include <python-kdevelop/Python.h>

#include <python-kdevelop/Python-ast.h>
#include <python-kdevelop/ast.h>

#include <python-kdevelop/graminit.h>
#include <python-kdevelop/grammar.h>
#include <python-kdevelop/parsetok.h>

#include <python-kdevelop/unicodeobject.h>

#include <python-kdevelop/object.h>

// remove evil macros from headers which pollute the namespace (grr!)
#undef test
#undef decorators
#undef Attribute

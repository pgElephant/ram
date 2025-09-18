/**
 * @name PostgreSQL Extension Security Issues
 * @description Finds potential security issues in PostgreSQL extensions
 * @kind problem
 * @problem.severity warning
 * @precision medium
 * @id cpp/postgresql-extension-security
 * @tags security
 *       postgresql
 *       extension
 */

import cpp

from FunctionCall fc, Function f
where
  fc.getTarget() = f and
  (
    f.getName() = "strcpy" or
    f.getName() = "strcat" or
    f.getName() = "sprintf" or
    f.getName() = "gets"
  ) and
  fc.getFile().getBaseName().matches("%.c")
select fc, "Use of potentially unsafe function " + f.getName() + " in PostgreSQL extension. Consider using safer alternatives like strlcpy, strlcat, snprintf, or fgets."

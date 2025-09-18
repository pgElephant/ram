/**
 * @name Buffer Overflow Vulnerabilities
 * @description Finds potential buffer overflow vulnerabilities
 * @kind problem
 * @problem.severity error
 * @precision medium
 * @id cpp/buffer-overflow
 * @tags security
 *       buffer-overflow
 *       postgresql
 */

import cpp

from FunctionCall fc, Function f
where
  fc.getTarget() = f and
  (
    f.getName() = "strcpy" or
    f.getName() = "strcat" or
    f.getName() = "sprintf"
  ) and
  fc.getFile().getBaseName().matches("%.c")
select fc, "Potential buffer overflow: " + f.getName() + " does not check buffer bounds. Consider using safer alternatives."

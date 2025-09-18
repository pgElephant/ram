/**
 * @name Memory Management Issues
 * @description Finds potential memory management issues in C code
 * @kind problem
 * @problem.severity error
 * @precision high
 * @id cpp/memory-management-issues
 * @tags security
 *       memory
 *       postgresql
 */

import cpp

from FunctionCall malloc_call, FunctionCall free_call
where
  malloc_call.getTarget().getName() = "malloc" and
  not exists(FunctionCall fc |
    fc.getTarget().getName() = "free" and
    fc.getAnArgument() = malloc_call
  )
select malloc_call, "Potential memory leak: malloc call without corresponding free."
